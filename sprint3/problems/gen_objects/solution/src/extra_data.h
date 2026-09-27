#pragma once

#include "model.h"

#include <boost/json.hpp>

#include <unordered_map>

namespace extra_data {

namespace json = boost::json;

class ExtraData {
public:
    void AddLootTypes(const model::Map::Id& map_id, json::array loot_types) {
        loot_types_by_map_id_[map_id] = std::move(loot_types);
    }

    const json::array& GetLootTypes(const model::Map::Id& map_id) const {
        static const json::array empty;

        if (auto it = loot_types_by_map_id_.find(map_id); it != loot_types_by_map_id_.end()) {
            return it->second;
        }

        return empty;
    }

private:
    std::unordered_map<model::Map::Id, json::array, util::TaggedHasher<model::Map::Id>>
        loot_types_by_map_id_;
};

}  // namespace extra_data
