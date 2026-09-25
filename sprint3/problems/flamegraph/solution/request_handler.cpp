#include "request_handler.h"

namespace http_handler {

std::string RequestHandler::MakeMapsListResponseBody() const {
    boost::json::array arr;
    for (const auto& map : game_.GetMaps()) {
        boost::json::object obj;
        obj["id"] = *map.GetId();
        obj["name"] = map.GetName();
        arr.push_back(std::move(obj));
    }
    return boost::json::serialize(arr);
}

std::string RequestHandler::MakeMapResponseBody(const model::Map& map) const {
    boost::json::object map_obj;
    map_obj["id"] = *map.GetId();
    map_obj["name"] = map.GetName();

    boost::json::array roads_arr;
    for (const auto& road : map.GetRoads()) {
        boost::json::object road_obj;
        road_obj["x0"] = road.GetStart().x;
        road_obj["y0"] = road.GetStart().y;
        if (road.IsHorizontal()) {
            road_obj["x1"] = road.GetEnd().x;
        } else {
            road_obj["y1"] = road.GetEnd().y;
        }
        roads_arr.push_back(std::move(road_obj));
    }
    map_obj["roads"] = std::move(roads_arr);

    boost::json::array buildings_arr;
    for (const auto& building : map.GetBuildings()) {
        boost::json::object b_obj;
        const auto& bounds = building.GetBounds();
        b_obj["x"] = bounds.position.x;
        b_obj["y"] = bounds.position.y;
        b_obj["w"] = bounds.size.width;
        b_obj["h"] = bounds.size.height;
        buildings_arr.push_back(std::move(b_obj));
    }
    map_obj["buildings"] = std::move(buildings_arr);

    boost::json::array offices_arr;
    for (const auto& office : map.GetOffices()) {
        boost::json::object o_obj;
        o_obj["id"] = *office.GetId();
        o_obj["x"] = office.GetPosition().x;
        o_obj["y"] = office.GetPosition().y;
        o_obj["offsetX"] = office.GetOffset().dx;
        o_obj["offsetY"] = office.GetOffset().dy;
        offices_arr.push_back(std::move(o_obj));
    }
    map_obj["offices"] = std::move(offices_arr);

    return boost::json::serialize(map_obj);
}

}
