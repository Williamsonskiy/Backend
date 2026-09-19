#include "json_loader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <boost/json.hpp>

namespace json_loader {

namespace json = boost::json;

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open JSON file: " + json_path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string file_content = buffer.str();

    json::value value = json::parse(file_content);
    json::object root = value.as_object();

    model::Game game;

    for (const auto& map_val : root.at("maps").as_array()) {
        const auto& map_obj = map_val.as_object();

        std::string id_str = json::value_to<std::string>(map_obj.at("id"));
        std::string name_str = json::value_to<std::string>(map_obj.at("name"));

        model::Map map(model::Map::Id{id_str}, name_str);

        // Roads
        for (const auto& road_val : map_obj.at("roads").as_array()) {
            const auto& road_obj = road_val.as_object();
            int x0 = json::value_to<int>(road_obj.at("x0"));
            int y0 = json::value_to<int>(road_obj.at("y0"));

            if (road_obj.contains("x1")) {
                int x1 = json::value_to<int>(road_obj.at("x1"));
                map.AddRoad(model::Road(model::Road::HORIZONTAL, {x0, y0}, x1));
            } else if (road_obj.contains("y1")) {
                int y1 = json::value_to<int>(road_obj.at("y1"));
                map.AddRoad(model::Road(model::Road::VERTICAL, {x0, y0}, y1));
            }
        }

        // Buildings
        if (map_obj.contains("buildings")) {
            for (const auto& building_val : map_obj.at("buildings").as_array()) {
                const auto& b_obj = building_val.as_object();
                int x = json::value_to<int>(b_obj.at("x"));
                int y = json::value_to<int>(b_obj.at("y"));
                int w = json::value_to<int>(b_obj.at("w"));
                int h = json::value_to<int>(b_obj.at("h"));

                map.AddBuilding(model::Building(model::Rectangle{{x, y}, {w, h}}));
            }
        }

        // Offices
        if (map_obj.contains("offices")) {
            for (const auto& office_val : map_obj.at("offices").as_array()) {
                const auto& o_obj = office_val.as_object();
                std::string o_id = json::value_to<std::string>(o_obj.at("id"));
                int x = json::value_to<int>(o_obj.at("x"));
                int y = json::value_to<int>(o_obj.at("y"));
                int ox = json::value_to<int>(o_obj.at("offsetX"));
                int oy = json::value_to<int>(o_obj.at("offsetY"));

                map.AddOffice(model::Office(model::Office::Id{o_id}, {x, y}, {ox, oy}));
            }
        }

        game.AddMap(std::move(map));
    }

    return game;
}

}  // namespace json_loader
