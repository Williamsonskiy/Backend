#include "model.h"
#include <stdexcept>

namespace model {

using namespace std::string_literals;

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse"s);
    }
    const size_t index = offices_.size();
    auto [it, inserted] = warehouse_id_to_index_.emplace(office.GetId(), index);
    try {
        offices_.push_back(std::move(office));
    } catch (...) {
        warehouse_id_to_index_.erase(it);
        throw;
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    }
    try {
        maps_.push_back(std::move(map));
    } catch (...) {
        map_id_to_index_.erase(map.GetId());
        throw;
    }
}

}  // namespace model
