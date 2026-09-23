#include "json_loader.h"

#include <boost/json.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace json_loader {

namespace json = boost::json;

namespace {

using namespace std::literals;

constexpr std::string_view kOpenConfigError = "Failed to open config file"sv;
constexpr std::string_view kParseConfigError = "Failed to parse JSON config file"sv;

int ReadInt(const json::object& obj, std::string_view key) {
    return static_cast<int>(obj.at(key.data()).as_int64());
}

double ReadDouble(const json::object& obj, std::string_view key) {
    const auto& value = obj.at(key.data());

    if (value.is_double()) {
        return value.as_double();
    }

    if (value.is_int64()) {
        return static_cast<double>(value.as_int64());
    }

    if (value.is_uint64()) {
        return static_cast<double>(value.as_uint64());
    }

    throw std::runtime_error("Expected number");
}

std::string ReadString(const json::object& obj, std::string_view key) {
    return std::string(obj.at(key.data()).as_string().c_str());
}

model::Road ParseRoad(const json::object& road_obj) {
    const model::Point start{
        ReadInt(road_obj, "x0"),
        ReadInt(road_obj, "y0")
    };

    if (road_obj.contains("x1")) {
        return model::Road(model::Road::HORIZONTAL, start, ReadInt(road_obj, "x1"));
    }

    return model::Road(model::Road::VERTICAL, start, ReadInt(road_obj, "y1"));
}

model::Building ParseBuilding(const json::object& building_obj) {
    return model::Building(model::Rectangle{
        {ReadInt(building_obj, "x"), ReadInt(building_obj, "y")},
        {ReadInt(building_obj, "w"), ReadInt(building_obj, "h")}
    });
}

model::Office ParseOffice(const json::object& office_obj) {
    return model::Office(
        model::Office::Id(ReadString(office_obj, "id")),
        {ReadInt(office_obj, "x"), ReadInt(office_obj, "y")},
        {ReadInt(office_obj, "offsetX"), ReadInt(office_obj, "offsetY")});
}

model::Map ParseMap(const json::object& map_obj, double default_dog_speed) {
    model::Map map(
        model::Map::Id(ReadString(map_obj, "id")),
        ReadString(map_obj, "name"));

    double dog_speed = default_dog_speed;

    if (map_obj.contains("dogSpeed")) {
        dog_speed = ReadDouble(map_obj, "dogSpeed");
    }

    map.SetDogSpeed(dog_speed);

    for (const auto& road_value : map_obj.at("roads").as_array()) {
        map.AddRoad(ParseRoad(road_value.as_object()));
    }

    for (const auto& building_value : map_obj.at("buildings").as_array()) {
        map.AddBuilding(ParseBuilding(building_value.as_object()));
    }

    for (const auto& office_value : map_obj.at("offices").as_array()) {
        map.AddOffice(ParseOffice(office_value.as_object()));
    }

    return map;
}

}  // namespace

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::ifstream input(json_path);
    if (!input.is_open()) {
        throw std::runtime_error(std::string(kOpenConfigError) + ": " + json_path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    json::value root;
    try {
        root = json::parse(buffer.str());
    } catch (const std::exception& ex) {
        throw std::runtime_error(
            std::string(kParseConfigError) + " '" + json_path.string() + "': " + ex.what());
    }

    const auto& root_obj = root.as_object();

    double default_dog_speed = 1.0;

    if (root_obj.contains("defaultDogSpeed")) {
        default_dog_speed = ReadDouble(root_obj, "defaultDogSpeed");
    }

    model::Game game;

    for (const auto& map_value : root_obj.at("maps").as_array()) {
        game.AddMap(ParseMap(map_value.as_object(), default_dog_speed));
    }

    return game;
}

}  // namespace json_loader
