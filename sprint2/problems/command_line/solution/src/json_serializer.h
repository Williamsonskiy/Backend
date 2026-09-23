#pragma once

#include "model.h"

#include <boost/json.hpp>

namespace json_serializer {

namespace json = boost::json;

json::value SerializeMaps(const model::Game::Maps& maps);
json::value SerializeMap(const model::Map& map);

json::object SerializeRoad(const model::Road& road);
json::array SerializeRoads(const model::Map::Roads& roads);

json::object SerializeBuilding(const model::Building& building);
json::array SerializeBuildings(const model::Map::Buildings& buildings);

json::object SerializeOffice(const model::Office& office);
json::array SerializeOffices(const model::Map::Offices& offices);

}  // namespace json_serializer
