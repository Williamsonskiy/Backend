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
    } else {
        double start_y = std::min(road.GetStart().y, road.GetEnd().y);
        double end_y = std::max(road.GetStart().y, road.GetEnd().y);
        return {static_cast<double>(road.GetStart().x), GetRandomDouble(start_y, end_y)};
    }
}

Dog* GameSession::AddDog(const std::string& name) {
    dogs_.emplace_back(dog_id_counter_++, name, GetRandomRoadPosition());
    return &dogs_.back();
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
    sessions_.emplace_back(map);
    try {
        map_id_to_session_index_[map_id] = index;
    } catch (...) {
        sessions_.pop_back();
        throw;
    }
    return &sessions_.back();
}

}  // namespace model