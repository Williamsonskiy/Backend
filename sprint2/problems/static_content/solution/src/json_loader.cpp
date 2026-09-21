#include "json_loader.h"

#include <fstream>
#include <sstream>
#include <string>
#include <boost/json.hpp>

namespace json_loader {

namespace {

void LoadRoads(model::Map& map, const boost::json::object& map_obj) {
    if (!map_obj.contains("roads")) {
        return;
    }
    for (const auto& road_val : map_obj.at("roads").as_array()) {
        const auto& road_obj = road_val.as_object();
        model::Coord x0 = road_obj.at("x0").as_int64();
        model::Coord y0 = road_obj.at("y0").as_int64();
        if (road_obj.contains("x1")) {
            model::Coord x1 = road_obj.at("x1").as_int64();
            map.AddRoad(model::Road(model::Road::HORIZONTAL, {x0, y0}, x1));
        } else if (road_obj.contains("y1")) {
            model::Coord y1 = road_obj.at("y1").as_int64();
            map.AddRoad(model::Road(model::Road::VERTICAL, {x0, y0}, y1));
        }
    }
}

void LoadBuildings(model::Map& map, const boost::json::object& map_obj) {
    if (!map_obj.contains("buildings")) {
        return;
    }
    for (const auto& building_val : map_obj.at("buildings").as_array()) {
        const auto& building_obj = building_val.as_object();
        model::Coord x = building_obj.at("x").as_int64();
        model::Coord y = building_obj.at("y").as_int64();
        model::Dimension w = building_obj.at("w").as_int64();
        model::Dimension h = building_obj.at("h").as_int64();
        map.AddBuilding(model::Building({ {x, y}, {w, h} }));
    }
}

void LoadOffices(model::Map& map, const boost::json::object& map_obj) {
    if (!map_obj.contains("offices")) {
        return;
    }
    for (const auto& office_val : map_obj.at("offices").as_array()) {
        const auto& office_obj = office_val.as_object();
        std::string office_id = std::string(office_obj.at("id").as_string());
        model::Coord x = office_obj.at("x").as_int64();
        model::Coord y = office_obj.at("y").as_int64();
        model::Dimension offset_x = office_obj.at("offsetX").as_int64();
        model::Dimension offset_y = office_obj.at("offsetY").as_int64();
        map.AddOffice(model::Office(model::Office::Id{std::move(office_id)}, {x, y}, {offset_x, offset_y}));
    }
}

} // namespace

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + json_path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    boost::json::value const jv = boost::json::parse(buffer.str());
    const auto& doc = jv.as_object();

    model::Game game;

    if (!doc.contains("maps")) {
        return game;
    }

    for (const auto& map_val : doc.at("maps").as_array()) {
        const auto& map_obj = map_val.as_object();
        std::string id = std::string(map_obj.at("id").as_string());
        std::string name = std::string(map_obj.at("name").as_string());

        model::Map map(model::Map::Id{std::move(id)}, std::move(name));

        LoadRoads(map, map_obj);
        LoadBuildings(map, map_obj);
        LoadOffices(map, map_obj);

        game.AddMap(std::move(map));
    }

    return game;
}

}
