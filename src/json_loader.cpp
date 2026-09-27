#include "json_loader.h"
#include <iostream>

namespace json_loader {

void AddRoads(const boost::json::value& map_json, model::Map& map) {
    for (const auto& road_json : map_json.at("roads").as_array()) {
        if (road_json.as_object().contains("x1")) {
            map.AddRoad(model::Road(
                model::Road::HORIZONTAL,
                {
                    model::Coord(road_json.at("x0").as_int64()), model::Coord(road_json.at("y0").as_int64())},
                    model::Coord(road_json.at("x1").as_int64())
                ));
        } else if (road_json.as_object().contains("y1")) {
            map.AddRoad(model::Road(
                model::Road::VERTICAL,
                {
                    model::Coord(road_json.at("x0").as_int64()), model::Coord(road_json.at("y0").as_int64())},
                    model::Coord(road_json.at("y1").as_int64())
                ));
        }
    }
}

void AddBuildings(const boost::json::value& map_json, model::Map& map) {
    for (const auto& building_json : map_json.at("buildings").as_array()) {
        model::Point position{
            model::Coord(building_json.at("x").as_int64()),
            model::Coord(building_json.at("y").as_int64())
        };

        model::Size s{
            model::Dimension(building_json.at("w").as_int64()),
            model::Dimension(building_json.at("h").as_int64())
        };

        model::Rectangle bounds{position, s};
        map.AddBuilding(model::Building(bounds));
    }
}

void AddOffices(const boost::json::value& map_json, model::Map& map) {
    for (const auto& office_json : map_json.at("offices").as_array()) {
        map.AddOffice(model::Office(
            model::Office::Id(std::string(office_json.at("id").as_string())),
            {model::Coord(office_json.at("x").as_int64()), model::Coord(office_json.at("y").as_int64())}, 
            {model::Dimension(office_json.at("offsetX").as_int64()), model::Dimension(office_json.at("offsetY").as_int64())}
        ));
    }
}

model::Game LoadGame(const std::filesystem::path& json_path) {

    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Не удалось открыть файл: " + json_path.string());
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string json_string = ss.str();
    file.close();

    boost::json::value json_value;
    try {
        json_value = boost::json::parse(json_string);
    } catch (...) {
        std::cerr << "Unknown exception during JSON parsing" << std::endl;
    }

    double default_dog_speed = 1.0;
    if (json_value.as_object().contains("defaultDogSpeed")) {
        const auto& v = json_value.as_object().at("defaultDogSpeed");
        default_dog_speed = v.is_double() ? v.as_double() : static_cast<double>(v.as_int64());
    }

    model::Game game;

    for (const auto& map_json : json_value.as_object().at("maps").as_array()) {
        double dog_speed = default_dog_speed;
        if (map_json.as_object().contains("dogSpeed")) {
            const auto& v = map_json.as_object().at("dogSpeed");
            dog_speed = v.is_double() ? v.as_double() : static_cast<double>(v.as_int64());
        }

        model::Map map(
            model::Map::Id(std::string(map_json.at("id").as_string())),
            std::string(map_json.at("name").as_string()),
            dog_speed
        );

        AddRoads(map_json, map);
        AddBuildings(map_json, map);
        AddOffices(map_json, map);

        game.AddMap(std::move(map));
    }

    return game;
}

}  // namespace json_loader