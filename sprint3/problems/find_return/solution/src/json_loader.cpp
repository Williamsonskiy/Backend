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

std::pair<model::Game, std::unordered_map<std::string, app::MapExtraData>> LoadGame(const std::filesystem::path& json_path) {

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

    double loot_period = 5.0;
    double loot_prob = 0.5;
    if (json_value.as_object().contains("lootGeneratorConfig")) {
        const auto& config = json_value.as_object().at("lootGeneratorConfig").as_object();
        const auto& period_val = config.at("period");
        loot_period = period_val.is_double() ? period_val.as_double() : static_cast<double>(period_val.as_int64());
        
        const auto& prob_val = config.at("probability");
        loot_prob = prob_val.is_double() ? prob_val.as_double() : static_cast<double>(prob_val.as_int64());
    }

    size_t default_bag_capacity = 3;
    if (json_value.as_object().contains("defaultBagCapacity")) {
        const auto& v = json_value.as_object().at("defaultBagCapacity");
        default_bag_capacity = v.is_uint64() ? v.as_uint64() : static_cast<size_t>(v.as_int64());
    }

    model::Game game;
    game.SetLootParameters(loot_period, loot_prob);

    std::unordered_map<std::string, app::MapExtraData> extra_data;

    for (const auto& map_json : json_value.as_object().at("maps").as_array()) {
        double dog_speed = default_dog_speed;
        if (map_json.as_object().contains("dogSpeed")) {
            const auto& v = map_json.as_object().at("dogSpeed");
            dog_speed = v.is_double() ? v.as_double() : static_cast<double>(v.as_int64());
        }

        size_t bag_capacity = default_bag_capacity;
        if (map_json.as_object().contains("bagCapacity")) {
            const auto& v = map_json.as_object().at("bagCapacity");
            bag_capacity = v.is_uint64() ? v.as_uint64() : static_cast<size_t>(v.as_int64());
        }

        size_t num_loot_types = 0;
        if (map_json.as_object().contains("lootTypes")) {
            const auto& loot_types = map_json.as_object().at("lootTypes").as_array();
            num_loot_types = loot_types.size();
            extra_data[std::string(map_json.at("id").as_string())].loot_types = loot_types;
        }

        model::Map map(
            model::Map::Id(std::string(map_json.at("id").as_string())),
            std::string(map_json.at("name").as_string()),
            dog_speed,
            num_loot_types,
            bag_capacity
        );

        AddRoads(map_json, map);
        AddBuildings(map_json, map);
        AddOffices(map_json, map);

        game.AddMap(std::move(map));
    }

    return {std::move(game), std::move(extra_data)};
}

}  // namespace json_loader
