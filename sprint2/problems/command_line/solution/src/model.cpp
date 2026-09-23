#include "model.h"
#include <stdexcept>
#include <random>
#include <algorithm>

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

Point2D GameSession::GetSpawnPosition() const {
    const auto& roads = map_->GetRoads();
    if (roads.empty()) {
        return {0.0, 0.0};
    }
    
    if (!random_spawn_) {
        return {static_cast<double>(roads.front().GetStart().x), 
                static_cast<double>(roads.front().GetStart().y)};
    }
    
    const auto& road = roads[GetRandomIndex(roads.size() - 1)];
    
    if (road.IsHorizontal()) {
        double start_x = std::min(road.GetStart().x, road.GetEnd().x);
        double end_x = std::max(road.GetStart().x, road.GetEnd().x);
        return {GetRandomDouble(start_x, end_x), static_cast<double>(road.GetStart().y)};
    } else {
        double start_y = std::min(road.GetStart().y, road.GetEnd().y);
        double end_y = std::max(road.GetStart().y, road.GetEnd().y);
        return {static_cast<double>(road.GetStart().x), GetRandomDouble(start_y, end_y)};
    }
}

Dog* GameSession::AddDog(const std::string& name) {
    dogs_.emplace_back(dog_id_counter_++, name, GetSpawnPosition());
    return &dogs_.back();
}

void GameSession::Tick(double delta_s) {
    for (auto& dog : dogs_) {
        auto speed = dog.GetSpeed();
        if (speed.ux == 0.0 && speed.uy == 0.0) continue;
        
        auto pos = dog.GetPosition();
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
            
            if (horizontal) {
                if (positive) { 
                    if (bound_x <= pos.x + 1e-8) { dog.SetSpeed({0.0, 0.0}); break; }
                    if (target_x <= bound_x) { pos.x = target_x; break; }
                    pos.x = bound_x;
                } else { 
                    if (bound_x >= pos.x - 1e-8) { dog.SetSpeed({0.0, 0.0}); break; }
                    if (target_x >= bound_x) { pos.x = target_x; break; }
                    pos.x = bound_x;
                }
            } else {
                if (positive) { 
                    if (bound_y <= pos.y + 1e-8) { dog.SetSpeed({0.0, 0.0}); break; }
                    if (target_y <= bound_y) { pos.y = target_y; break; }
                    pos.y = bound_y;
                } else { 
                    if (bound_y >= pos.y - 1e-8) { dog.SetSpeed({0.0, 0.0}); break; }
                    if (target_y >= bound_y) { pos.y = target_y; break; }
                    pos.y = bound_y;
                }
            }
        }
        dog.SetPosition(pos);
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
    sessions_.emplace_back(map, random_spawn_);
    try {
        map_id_to_session_index_[map_id] = index;
    } catch (...) {
        sessions_.pop_back();
        throw;
    }
    return &sessions_.back();
}

void Game::Tick(std::chrono::milliseconds delta) {
    double delta_s = delta.count() / 1000.0;
    for (auto& session : sessions_) {
        session.Tick(delta_s);
    }
}

}  // namespace model
