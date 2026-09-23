#include "json_serializer.h"

#include <utility>

namespace json_serializer {

json::value SerializeMaps(const model::Game::Maps& maps) {
    json::array result;

    for (const auto& map : maps) {
        json::object map_obj;
        map_obj["id"] = *map.GetId();
        map_obj["name"] = map.GetName();

        result.emplace_back(std::move(map_obj));
    }

    return result;
}

json::value SerializeMap(const model::Map& map) {
    json::object obj;

    obj["id"] = *map.GetId();
    obj["name"] = map.GetName();
    obj["roads"] = SerializeRoads(map.GetRoads());
    obj["buildings"] = SerializeBuildings(map.GetBuildings());
    obj["offices"] = SerializeOffices(map.GetOffices());

    return obj;
}

json::object SerializeRoad(const model::Road& road) {
    json::object obj;

    const auto start = road.GetStart();
    const auto end = road.GetEnd();

    obj["x0"] = start.x;
    obj["y0"] = start.y;

    if (road.IsHorizontal()) {
        obj["x1"] = end.x;
    } else {
        obj["y1"] = end.y;
    }

    return obj;
}

json::array SerializeRoads(const model::Map::Roads& roads) {
    json::array result;

    for (const auto& road : roads) {
        result.emplace_back(SerializeRoad(road));
    }

    return result;
}

json::object SerializeBuilding(const model::Building& building) {
    json::object obj;

    const auto& bounds = building.GetBounds();

    obj["x"] = bounds.position.x;
    obj["y"] = bounds.position.y;
    obj["w"] = bounds.size.width;
    obj["h"] = bounds.size.height;

    return obj;
}

json::array SerializeBuildings(const model::Map::Buildings& buildings) {
    json::array result;

    for (const auto& building : buildings) {
        result.emplace_back(SerializeBuilding(building));
    }

    return result;
}

json::object SerializeOffice(const model::Office& office) {
    json::object obj;

    const auto position = office.GetPosition();
    const auto offset = office.GetOffset();

    obj["id"] = *office.GetId();
    obj["x"] = position.x;
    obj["y"] = position.y;
    obj["offsetX"] = offset.dx;
    obj["offsetY"] = offset.dy;

    return obj;
}

json::array SerializeOffices(const model::Map::Offices& offices) {
    json::array result;

    for (const auto& office : offices) {
        result.emplace_back(SerializeOffice(office));
    }

    return result;
}

}  // namespace json_serializer
