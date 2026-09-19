#pragma once
#include "model.h"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <string_view>
#include <utility>
#include <string>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;
using namespace std::literals;

struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view APPLICATION_JSON = "application/json"sv;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
};

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            send(MakeJsonResponse(http::status::method_not_allowed,
                                 MakeErrorJson("invalidMethod", "Invalid method"),
                                 req.version(), req.keep_alive(),
                                 req.method(), "GET, HEAD"sv));
            return;
        }

        // ИСПРАВЛЕНО: Явное создание std::string_view из boost::beast::string_view
        std::string_view target{req.target().data(), req.target().size()};

        if (!target.starts_with("/api/v1/maps")) {
            send(MakeJsonResponse(http::status::bad_request,
                                 MakeErrorJson("badRequest", "Bad request"),
                                 req.version(), req.keep_alive(),
                                 req.method()));
            return;
        }

        if (target == "/api/v1/maps" || target == "/api/v1/maps/") {
            send(HandleGetMaps(req));
            return;
        }

        constexpr std::string_view prefix = "/api/v1/maps/";
        if (target.starts_with(prefix)) {
            std::string_view map_id_str = target.substr(prefix.size());
            send(HandleGetMapById(map_id_str, req));
            return;
        }

        send(MakeJsonResponse(http::status::bad_request,
                             MakeErrorJson("badRequest", "Bad request"),
                             req.version(), req.keep_alive(),
                             req.method()));
    }

private:
    model::Game& game_;

    template <typename Body, typename Allocator>
    http::response<http::string_body> HandleGetMaps(
        const http::request<Body, http::basic_fields<Allocator>>& req) const {
        json::array arr;
        for (const auto& map : game_.GetMaps()) {
            json::object obj;
            obj["id"] = *map.GetId();
            obj["name"] = map.GetName();
            arr.push_back(std::move(obj));
        }
        return MakeJsonResponse(http::status::ok, json::serialize(arr),
                                req.version(), req.keep_alive(), req.method());
    }

    template <typename Body, typename Allocator>
    http::response<http::string_body> HandleGetMapById(
        std::string_view map_id_str,
        const http::request<Body, http::basic_fields<Allocator>>& req) const {
        model::Map::Id id{std::string(map_id_str)};
        const auto* map = game_.FindMap(id);

        if (!map) {
            return MakeJsonResponse(http::status::not_found,
                                    MakeErrorJson("mapNotFound", "Map not found"),
                                    req.version(), req.keep_alive(), req.method());
        }

        json::object obj;
        obj["id"] = *map->GetId();
        obj["name"] = map->GetName();

        // Roads
        json::array roads;
        for (const auto& road : map->GetRoads()) {
            json::object road_obj;
            road_obj["x0"] = road.GetStart().x;
            road_obj["y0"] = road.GetStart().y;
            if (road.IsHorizontal()) {
                road_obj["x1"] = road.GetEnd().x;
            } else {
                road_obj["y1"] = road.GetEnd().y;
            }
            roads.push_back(std::move(road_obj));
        }
        obj["roads"] = std::move(roads);

        // Buildings
        json::array buildings;
        for (const auto& building : map->GetBuildings()) {
            const auto& bounds = building.GetBounds();
            json::object b_obj;
            b_obj["x"] = bounds.position.x;
            b_obj["y"] = bounds.position.y;
            b_obj["w"] = bounds.size.width;
            b_obj["h"] = bounds.size.height;
            buildings.push_back(std::move(b_obj));
        }
        obj["buildings"] = std::move(buildings);

        // Offices
        json::array offices;
        for (const auto& office : map->GetOffices()) {
            json::object off_obj;
            off_obj["id"] = *office.GetId();
            off_obj["x"] = office.GetPosition().x;
            off_obj["y"] = office.GetPosition().y;
            off_obj["offsetX"] = office.GetOffset().dx;
            off_obj["offsetY"] = office.GetOffset().dy;
            offices.push_back(std::move(off_obj));
        }
        obj["offices"] = std::move(offices);

        return MakeJsonResponse(http::status::ok, json::serialize(obj),
                                req.version(), req.keep_alive(), req.method());
    }

    static std::string MakeErrorJson(std::string_view code, std::string_view message);

    static http::response<http::string_body> MakeJsonResponse(
        http::status status,
        std::string body,
        unsigned version,
        bool keep_alive,
        http::verb method,
        std::string_view allow_header = {}
    );
};

}  // namespace http_handler
