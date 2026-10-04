#include "model.h"
#include <stdexcept>
#include <random>
#include <algorithm>
#include <unordered_set>
#include "collision_detector.h"

namespace model {
using namespace std::literals;

namespace {
    double GetRandomDouble(double min, double max) {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dist(min, max);
        return dist(gen);
    }
    size_t GetRandomIndex(size_t max) {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dist(0, max);
        return dist(gen);
    }
}

Point2D GameSession::GetRandomRoadPosition() const {
    const auto& roads = map_->GetRoads();
    if (roads.empty()) {
        return {0.0, 0.0};
    }
    const auto& road = roads[GetRandomIndex(roads.size() - 1)];
    
    if (road.IsHorizontal()) {
        double start_x = std::min(road.GetStart().x, road.GetEnd().x);
        double end_x = std::max(road.GetStart().x, road.GetEnd().x);
        return {GetRandomDouble(start_x, end_x), static_cast<double>(road.GetStart().y)};
    }
    
    double start_y = std::min(road.GetStart().y, road.GetEnd().y);
    double end_y = std::max(road.GetStart().y, road.GetEnd().y);
    return {static_cast<double>(road.GetStart().x), GetRandomDouble(start_y, end_y)};
}

Point2D GameSession::GetSpawnPosition() const {
    if (!random_spawn_) {
        const auto& roads = map_->GetRoads();
        if (roads.empty()) {
            return {0.0, 0.0};
        }
        return {static_cast<double>(roads.front().GetStart().x), 
                static_cast<double>(roads.front().GetStart().y)};
    }
    return GetRandomRoadPosition();
}

Dog* GameSession::AddDog(const std::string& name) {
    size_t id = dog_id_counter_++;
    auto [it, inserted] = dogs_.emplace(id, Dog(id, name, GetSpawnPosition()));
    return &it->second;
}

class SessionItemGathererProvider : public collision_detector::ItemGathererProvider {
public:
    struct ItemInfo {
        bool is_office;
        size_t id; 
        size_t type; 
    };

    SessionItemGathererProvider(const std::vector<collision_detector::Item>& items,
                                const std::vector<collision_detector::Gatherer>& gatherers,
                                const std::vector<ItemInfo>& item_infos,
                                const std::vector<Dog*>& dog_ptrs)
        : items_(items), gatherers_(gatherers), item_infos_(item_infos), dog_ptrs_(dog_ptrs) {}

    size_t ItemsCount() const override { return items_.size(); }
    collision_detector::Item GetItem(size_t idx) const override { return items_[idx]; }
    size_t GatherersCount() const override { return gatherers_.size(); }
    collision_detector::Gatherer GetGatherer(size_t idx) const override { return gatherers_[idx]; }

    const ItemInfo& GetItemInfo(size_t idx) const { return item_infos_[idx]; }
    Dog* GetDog(size_t idx) const { return dog_ptrs_[idx]; }

private:
    std::vector<collision_detector::Item> items_;
    std::vector<collision_detector::Gatherer> gatherers_;
    std::vector<ItemInfo> item_infos_;
    std::vector<Dog*> dog_ptrs_;
};

void GameSession::Tick(std::chrono::milliseconds delta) {
    double delta_s = delta.count() / 1000.0;
    
    std::vector<collision_detector::Gatherer> gatherers;
    std::vector<Dog*> dog_ptrs;
    
    for (auto& [id, dog] : dogs_) {
        dog.UpdatePlayTime(delta);
        auto speed = dog.GetSpeed();
        auto start_pos = dog.GetPosition();
        bool was_moving = (speed.ux != 0.0 || speed.uy != 0.0);
        
        Point2D pos = start_pos;
        if (was_moving) {
            double target_x = pos.x + speed.ux * delta_s;
            double target_y = pos.y + speed.uy * delta_s;
            
            while (true) {
                double bound_x = pos.x;
                double bound_y = pos.y;
                
                bool horizontal = speed.ux != 0.0;
                bool positive = horizontal ? (speed.ux > 0) : (speed.uy > 0);
                
                if (horizontal) {
                    bound_x = positive ? -1e9 : 1e9;
                } else {
                    bound_y = positive ? -1e9 : 1e9;
                }
                
                auto roads = map_->GetRoadsContaining(pos);
                if (roads.empty()) {
                    dog.SetSpeed({0.0, 0.0});
                    break;
                }
                
                for (const auto* road : roads) {
                    double min_x, max_x, min_y, max_y;
                    map_->GetRoadBounds(*road, min_x, max_x, min_y, max_y);
                    if (horizontal) {
                        bound_x = positive ? std::max(bound_x, max_x) : std::min(bound_x, min_x);
                    } else {
                        bound_y = positive ? std::max(bound_y, max_y) : std::min(bound_y, min_y);
                    }
                }
                
                double& current_pos = horizontal ? pos.x : pos.y;
                double target_pos = horizontal ? target_x : target_y;
                double bound = horizontal ? bound_x : bound_y;

                if (positive ? (bound <= current_pos + 1e-8) : (bound >= current_pos - 1e-8)) {
                    dog.SetSpeed({0.0, 0.0});
                    break;
                }
                
                if (positive ? (target_pos <= bound) : (target_pos >= bound)) {
                    current_pos = target_pos;
                    break;
                }
                
                current_pos = bound;
            }
            dog.SetPosition(pos);
        }
        
        if (was_moving) {
            dog.ResetIdleTime();
        } else {
            dog.UpdateIdleTime(delta);
        }
        
        gatherers.push_back({geom::Point2D{start_pos.x, start_pos.y}, geom::Point2D{pos.x, pos.y}, 0.3});
        dog_ptrs.push_back(&dog);
    }
    
    std::vector<collision_detector::Item> items;
    std::vector<SessionItemGathererProvider::ItemInfo> item_infos;
    
    for (const auto& [id, lo] : lost_objects_) {
        items.push_back({geom::Point2D{lo.pos.x, lo.pos.y}, 0.0}); 
        item_infos.push_back({false, lo.id, lo.type});
    }
    
    for (const auto& office : map_->GetOffices()) {
        geom::Point2D pos{static_cast<double>(office.GetPosition().x), static_cast<double>(office.GetPosition().y)};
        items.push_back({pos, 0.25}); 
        item_infos.push_back({true, 0, 0}); 
    }
    
    SessionItemGathererProvider provider(items, gatherers, item_infos, dog_ptrs);
    auto events = collision_detector::FindGatherEvents(provider);
    
    std::unordered_set<size_t> collected_items;
    size_t bag_capacity = map_->GetBagCapacity();
    
    for (const auto& event : events) {
        Dog* dog = provider.GetDog(event.gatherer_id);
        const auto& item_info = provider.GetItemInfo(event.item_id);
        
        if (item_info.is_office) {
            for (const auto& item : dog->GetBag()) {
                dog->AddScore(map_->GetLootValue(item.type));
            }
            dog->EmptyBag();
        } else {
            if (collected_items.contains(item_info.id)) continue;
            
            if (dog->GetBag().size() < bag_capacity) {
                dog->PutToBag({item_info.id, item_info.type});
                collected_items.insert(item_info.id);
                lost_objects_.erase(item_info.id);
            }
        }
    }
    
    unsigned generated_loot = loot_generator_.Generate(delta, lost_objects_.size(), dogs_.size());
    if (generated_loot > 0 && map_->GetNumLootTypes() > 0) {
        for (unsigned i = 0; i < generated_loot; ++i) {
            size_t loot_type = GetRandomIndex(map_->GetNumLootTypes() - 1);
            Point2D pos = GetRandomRoadPosition();
            lost_objects_[lost_object_id_counter_] = LostObject{lost_object_id_counter_, loot_type, pos};
            lost_object_id_counter_++;
        }
    }
}

void Map::GetRoadBounds(const Road& road, double& min_x, double& max_x, double& min_y, double& max_y) const {
    constexpr double half_width = 0.4;
    if (road.IsHorizontal()) {
        min_x = std::min(road.GetStart().x, road.GetEnd().x) - half_width;
        max_x = std::max(road.GetStart().x, road.GetEnd().x) + half_width;
        min_y = road.GetStart().y - half_width;
        max_y = road.GetStart().y + half_width;
    } else {
        min_x = road.GetStart().x - half_width;
        max_x = road.GetStart().x + half_width;
        min_y = std::min(road.GetStart().y, road.GetEnd().y) - half_width;
        max_y = std::max(road.GetStart().y, road.GetEnd().y) + half_width;
    }
}

bool Map::IsPointInRoad(Point2D p, const Road& road) const {
    double min_x, max_x, min_y, max_y;
    GetRoadBounds(road, min_x, max_x, min_y, max_y);
    constexpr double EPSILON = 1e-4;
    return p.x >= min_x - EPSILON && p.x <= max_x + EPSILON &&
           p.y >= min_y - EPSILON && p.y <= max_y + EPSILON;
}

std::vector<const Road*> Map::GetRoadsContaining(Point2D p) const {
    std::vector<const Road*> result;
    int rounded_y = std::round(p.y);
    if (auto it = horizontal_roads_.find(rounded_y); it != horizontal_roads_.end()) {
        for (size_t idx : it->second) {
            if (IsPointInRoad(p, roads_[idx])) result.push_back(&roads_[idx]);
        }
    }
    int rounded_x = std::round(p.x);
    if (auto it = vertical_roads_.find(rounded_x); it != vertical_roads_.end()) {
        for (size_t idx : it->second) {
            if (IsPointInRoad(p, roads_[idx])) result.push_back(&roads_[idx]);
        }
    }
    return result;
}

void Map::AddRoad(const Road& road) {
    size_t idx = roads_.size();
    roads_.emplace_back(road);
    if (road.IsHorizontal()) {
        horizontal_roads_[road.GetStart().y].push_back(idx);
    } else {
        vertical_roads_[road.GetStart().x].push_back(idx);
    }
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        offices_.pop_back();
        throw;
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

GameSession* Game::GetSession(const Map::Id& map_id) {
    if (auto it = map_id_to_session_index_.find(map_id); it != map_id_to_session_index_.end()) {
        return &sessions_.at(it->second);
    }
    return nullptr;
}

GameSession* Game::AddSession(const Map::Id& map_id) {
    const Map* map = FindMap(map_id);
    if (!map) return nullptr;

    const size_t index = sessions_.size();
    sessions_.emplace_back(map, random_spawn_, loot_period_, loot_probability_);
    try {
        map_id_to_session_index_[map_id] = index;
    } catch (...) {
        sessions_.pop_back();
        throw;
    }
    return &sessions_.back();
}

void Game::Tick(std::chrono::milliseconds delta) {
    for (auto& session : sessions_) {
        session.Tick(delta);
    }
}

}  // namespace model
