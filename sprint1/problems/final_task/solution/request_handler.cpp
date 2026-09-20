#include "request_handler.h"

namespace http_handler {

namespace {

using namespace std::literals;

constexpr std::string_view KEY_ID = "id";
constexpr std::string_view KEY_NAME = "name";
constexpr std::string_view KEY_ROADS = "roads";
constexpr std::string_view KEY_BUILDINGS = "buildings";
constexpr std::string_view KEY_OFFICES = "offices";
constexpr std::string_view KEY_X0 = "x0";
constexpr std::string_view KEY_Y0 = "y0";
constexpr std::string_view KEY_X1 = "x1";
constexpr std::string_view KEY_Y1 = "y1";
constexpr std::string_view KEY_X = "x";
constexpr std::string_view KEY_Y = "y";
constexpr std::string_view KEY_W = "w";
constexpr std::string_view KEY_H = "h";
constexpr std::string_view KEY_OFFSET_X = "offsetX";
constexpr std::string_view KEY_OFFSET_Y = "offsetY";

boost::json::array SerializeRoads(const model::Map& map) {
    boost::json::array roads_arr;
    for (const auto& road : map.GetRoads()) {
        boost::json::object road_obj;
        road_obj[KEY_X0.data()] = road.GetStart().x;
        road_obj[KEY_Y0.data()] = road.GetStart().y;
        if (road.IsHorizontal()) {
            road_obj[KEY_X1.data()] = road.GetEnd().x;
        } else {
            road_obj[KEY_Y1.data()] = road.GetEnd().y;
        }
        roads_arr.push_back(std::move(road_obj));
    }
    return roads_arr;
}

boost::json::array SerializeBuildings(const model::Map& map) {
    boost::json::array buildings_arr;
    for (const auto& building : map.GetBuildings()) {
        boost::json::object b_obj;
        const auto& bounds = building.GetBounds();
        b_obj[KEY_X.data()] = bounds.position.x;
        b_obj[KEY_Y.data()] = bounds.position.y;
        b_obj[KEY_W.data()] = bounds.size.width;
        b_obj[KEY_H.data()] = bounds.size.height;
        buildings_arr.push_back(std::move(b_obj));
    }
    return buildings_arr;
}

boost::json::array SerializeOffices(const model::Map& map) {
    boost::json::array offices_arr;
    for (const auto& office : map.GetOffices()) {
        boost::json::object o_obj;
        o_obj[KEY_ID.data()] = *office.GetId();
        o_obj[KEY_X.data()] = office.GetPosition().x;
        o_obj[KEY_Y.data()] = office.GetPosition().y;
        o_obj[KEY_OFFSET_X.data()] = office.GetOffset().dx;
        o_obj[KEY_OFFSET_Y.data()] = office.GetOffset().dy;
        offices_arr.push_back(std::move(o_obj));
    }
    return offices_arr;
}

} // namespace

std::string RequestHandler::MakeMapsListResponseBody() const {
    boost::json::array arr;
    for (const auto& map : game_.GetMaps()) {
        boost::json::object obj;
        obj[KEY_ID.data()] = *map.GetId();
        obj[KEY_NAME.data()] = map.GetName();
        arr.push_back(std::move(obj));
    }
    return boost::json::serialize(arr);
}

std::string RequestHandler::MakeMapResponseBody(const model::Map& map) const {
    boost::json::object map_obj;
    map_obj[KEY_ID.data()] = *map.GetId();
    map_obj[KEY_NAME.data()] = map.GetName();
    map_obj[KEY_ROADS.data()] = SerializeRoads(map);
    map_obj[KEY_BUILDINGS.data()] = SerializeBuildings(map);
    map_obj[KEY_OFFICES.data()] = SerializeOffices(map);
    return boost::json::serialize(map_obj);
}

}
