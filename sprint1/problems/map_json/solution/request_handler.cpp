#include "request_handler.h"

namespace http_handler {

using namespace std::literals;

http::response<http::string_body> RequestHandler::MakeJsonResponse(
    http::status status,
    std::string_view json_body,
    unsigned http_version,
    bool keep_alive) const {

    http::response<http::string_body> response(status, http_version);
    response.set(http::field::content_type, ContentType::APPLICATION_JSON);
    response.body() = std::string(json_body);
    response.content_length(response.body().size());
    response.keep_alive(keep_alive);
    return response;
}

http::response<http::string_body> RequestHandler::MakeBadRequestResponse(
    unsigned http_version,
    bool keep_alive) const {

    json::object err_obj;
    err_obj["code"] = "badRequest";
    err_obj["message"] = "Bad request";

    return MakeJsonResponse(http::status::bad_request, json::serialize(err_obj), http_version, keep_alive);
}

http::response<http::string_body> RequestHandler::MakeNotFoundResponse(
    std::string_view code,
    std::string_view message,
    unsigned http_version,
    bool keep_alive) const {

    json::object err_obj;
    err_obj["code"] = std::string(code);
    err_obj["message"] = std::string(message);

    return MakeJsonResponse(http::status::not_found, json::serialize(err_obj), http_version, keep_alive);
}

std::string RequestHandler::SerializeMapList() const {
    json::array arr;
    for (const auto& map : game_.GetMaps()) {
        json::object obj;
        obj["id"] = *map.GetId();
        obj["name"] = map.GetName();
        arr.push_back(std::move(obj));
    }
    return json::serialize(arr);
}

std::string RequestHandler::SerializeMap(const model::Map& map) const {
    json::object map_obj;
    map_obj["id"] = *map.GetId();
    map_obj["name"] = map.GetName();

    // Roads
    json::array roads_arr;
    for (const auto& road : map.GetRoads()) {
        json::object r_obj;
        auto start = road.GetStart();
        auto end = road.GetEnd();
        r_obj["x0"] = start.x;
        r_obj["y0"] = start.y;

        if (road.IsHorizontal()) {
            r_obj["x1"] = end.x;
        } else {
            r_obj["y1"] = end.y;
        }
        roads_arr.push_back(std::move(r_obj));
    }
    map_obj["roads"] = std::move(roads_arr);

    // Buildings
    json::array buildings_arr;
    for (const auto& building : map.GetBuildings()) {
        json::object b_obj;
        const auto& bounds = building.GetBounds();
        b_obj["x"] = bounds.position.x;
        b_obj["y"] = bounds.position.y;
        b_obj["w"] = bounds.size.width;
        b_obj["h"] = bounds.size.height;
        buildings_arr.push_back(std::move(b_obj));
    }
    map_obj["buildings"] = std::move(buildings_arr);

    // Offices
    json::array offices_arr;
    for (const auto& office : map.GetOffices()) {
        json::object o_obj;
        o_obj["id"] = *office.GetId();
        o_obj["x"] = office.GetPosition().x;
        o_obj["y"] = office.GetPosition().y;
        o_obj["offsetX"] = office.GetOffset().dx;
        o_obj["offsetY"] = office.GetOffset().dy;
        offices_arr.push_back(std::move(o_obj));
    }
    map_obj["offices"] = std::move(offices_arr);

    return json::serialize(map_obj);
}

http::response<http::string_body> RequestHandler::HandleRequest(http::request<http::string_body>&& req) {
    const auto target = req.target();

    if (target.starts_with("/api/")) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return MakeBadRequestResponse(req.version(), req.keep_alive());
        }

        if (target == "/api/v1/maps" || target == "/api/v1/maps/") {
            return MakeJsonResponse(http::status::ok, SerializeMapList(), req.version(), req.keep_alive());
        }

        constexpr std::string_view prefix = "/api/v1/maps/";
        if (target.starts_with(prefix)) {
            std::string_view map_id_str = target.substr(prefix.size());
            if (map_id_str.empty()) {
                return MakeBadRequestResponse(req.version(), req.keep_alive());
            }

            model::Map::Id id{std::string(map_id_str)};
            if (const auto* map = game_.FindMap(id)) {
                return MakeJsonResponse(http::status::ok, SerializeMap(*map), req.version(), req.keep_alive());
            } else {
                return MakeNotFoundResponse("mapNotFound", "Map not found", req.version(), req.keep_alive());
            }
        }

        return MakeBadRequestResponse(req.version(), req.keep_alive());
    }

    return MakeBadRequestResponse(req.version(), req.keep_alive());
}

}  // namespace http_handler
