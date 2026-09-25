#pragma once
#include "app.h"
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <string_view>
#include <string>

namespace http_handler {

namespace http = boost::beast::http;
namespace json = boost::json;

class ApiHandler {
public:
    explicit ApiHandler(app::App& app) : app_(app) {}

    template <typename Body, typename Allocator, typename Send>
    void HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string_view target = req.target();

        if (target == "/api/v1/game/join") {
            return HandleJoinGame(std::move(req), std::forward<Send>(send));
        } else if (target == "/api/v1/game/players") {
            return HandleGetPlayers(std::move(req), std::forward<Send>(send));
        } else if (target == "/api/v1/game/state") {
            return HandleGetGameState(std::move(req), std::forward<Send>(send));
        } else if (target == "/api/v1/game/player/action") {
            return HandlePlayerAction(std::move(req), std::forward<Send>(send));
        } else if (target == "/api/v1/game/tick") {
            if (app_.IsAutoTick()) {
                return send(MakeErrorResponse(http::status::bad_request, "badRequest", "Invalid endpoint", req));
            } else {
                return HandleGameTick(std::move(req), std::forward<Send>(send));
            }
        } else if (target == "/api/v1/maps") {
            return HandleGetMaps(std::move(req), std::forward<Send>(send));
        } else if (target.starts_with("/api/v1/maps/")) {
            return HandleGetMap(std::move(req), std::forward<Send>(send));
        }
        
        return send(MakeErrorResponse(http::status::bad_request, "badRequest", "Bad request", req));
    }

private:
    app::App& app_;

    template <typename Request>
    auto MakeErrorResponse(http::status status, std::string_view code, std::string_view message, const Request& req, std::string_view allow_methods = "") {
        json::object obj = {{"code", code}, {"message", message}};
        auto res = MakeJsonResponse(status, json::serialize(obj), req);
        if (!allow_methods.empty()) {
            res.set(http::field::allow, allow_methods);
        }
        return res;
    }

    template <typename Request>
    http::response<http::string_body> MakeJsonResponse(http::status status, std::string_view body, const Request& req) {
        http::response<http::string_body> res(status, req.version());
        res.set(http::field::content_type, "application/json");
        res.set(http::field::cache_control, "no-cache");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(body);
        res.prepare_payload();
        return res;
    }

    template <typename Request, typename Send>
    void HandleJoinGame(Request&& req, Send&& send) {
        if (req.method() != http::verb::post) {
            return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Only POST method is expected", req, "POST"));
        }

        std::string user_name;
        std::string map_id_str;
        try {
            json::value jv = json::parse(req.body());
            if (!jv.is_object()) {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error", req));
            }
            const auto& obj = jv.as_object();
            if (!obj.contains("userName") || !obj.contains("mapId")) {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error", req));
            }
            user_name = json::value_to<std::string>(obj.at("userName"));
            map_id_str = json::value_to<std::string>(obj.at("mapId"));
        } catch (...) {
            return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error", req));
        }

        if (user_name.empty()) {
            return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid name", req));
        }

        model::Map::Id map_id{map_id_str};
        if (!app_.GetGame().FindMap(map_id)) {
            return send(MakeErrorResponse(http::status::not_found, "mapNotFound", "Map not found", req));
        }

        auto [token, player_id] = app_.JoinGame(user_name, map_id);
        json::object response_obj = {
            {"authToken", token},
            {"playerId", player_id}
        };

        send(MakeJsonResponse(http::status::ok, json::serialize(response_obj), req));
    }

    template <typename Request, typename Send>
    void HandleGetPlayers(Request&& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req, "GET, HEAD"));
        }

        auto auth_it = req.find(http::field::authorization);
        if (auth_it == req.end() || !auth_it->value().starts_with("Bearer ") || auth_it->value().size() != 39) {
            return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is missing", req));
        }

        std::string token_str(auth_it->value().substr(7));
        app::Player* player = app_.GetPlayerByToken(token_str);
        if (!player) {
            return send(MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", req));
        }

        json::object players_obj;
        for (const auto& dog : player->GetSession()->GetDogs()) {
            players_obj[std::to_string(dog.GetId())] = json::object{{"name", dog.GetName()}};
        }

        auto res = MakeJsonResponse(http::status::ok, json::serialize(players_obj), req);
        if (req.method() == http::verb::head) {
            res.body().clear();
            res.content_length(json::serialize(players_obj).size());
        }
        send(std::move(res));
    }

    template <typename Request, typename Send>
    void HandleGetGameState(Request&& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req, "GET, HEAD"));
        }

        auto auth_it = req.find(http::field::authorization);
        if (auth_it == req.end() || !auth_it->value().starts_with("Bearer ") || auth_it->value().size() != 39) {
            return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is required", req));
        }

        std::string token_str(auth_it->value().substr(7));
        app::Player* player = app_.GetPlayerByToken(token_str);
        if (!player) {
            return send(MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", req));
        }

        json::object players_obj;
        for (const auto& dog : player->GetSession()->GetDogs()) {
            json::object dog_obj;
            dog_obj["pos"] = json::array{dog.GetPosition().x, dog.GetPosition().y};
            dog_obj["speed"] = json::array{dog.GetSpeed().ux, dog.GetSpeed().uy};
            dog_obj["dir"] = std::string(model::DirectionToString(dog.GetDirection()));
            players_obj[std::to_string(dog.GetId())] = dog_obj;
        }

        json::object root;
        root["players"] = players_obj;

        auto res = MakeJsonResponse(http::status::ok, json::serialize(root), req);
        if (req.method() == http::verb::head) {
            res.body().clear();
            res.content_length(json::serialize(root).size());
        }
        send(std::move(res));
    }

    template <typename Request, typename Send>
    void HandlePlayerAction(Request&& req, Send&& send) {
        if (req.method() != http::verb::post) {
            return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req, "POST"));
        }

        auto ct_it = req.find(http::field::content_type);
        if (ct_it == req.end() || ct_it->value() != "application/json") {
            return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid content type", req));
        }

        auto auth_it = req.find(http::field::authorization);
        if (auth_it == req.end() || !auth_it->value().starts_with("Bearer ") || auth_it->value().size() != 39) {
            return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is required", req));
        }

        std::string token_str(auth_it->value().substr(7));
        app::Player* player = app_.GetPlayerByToken(token_str);
        if (!player) {
            return send(MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", req));
        }

        std::string move_cmd;
        try {
            json::value jv = json::parse(req.body());
            if (!jv.is_object() || !jv.as_object().contains("move")) {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", req));
            }
            move_cmd = json::value_to<std::string>(jv.as_object().at("move"));
            if (move_cmd != "U" && move_cmd != "D" && move_cmd != "L" && move_cmd != "R" && move_cmd != "") {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", req));
            }
        } catch (...) {
            return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", req));
        }

        model::Dog* dog = player->GetDog();
        double speed = player->GetSession()->GetMap()->GetDogSpeed();
        dog->Move(move_cmd, speed);

        json::object response_obj;
        send(MakeJsonResponse(http::status::ok, json::serialize(response_obj), req));
    }

    template <typename Request, typename Send>
    void HandleGameTick(Request&& req, Send&& send) {
        if (req.method() != http::verb::post) {
            return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Only POST method is expected", req, "POST"));
        }

        int time_delta = 0;
        try {
            json::value jv = json::parse(req.body());
            if (!jv.is_object() || !jv.as_object().contains("timeDelta")) {
                return send(MakeErrorResponse(http::status::bad_request, "badRequest", "Failed to parse tick request JSON", req));
            }
            
            const auto& val = jv.as_object().at("timeDelta");
            if (val.is_int64()) {
                time_delta = static_cast<int>(val.as_int64());
            } else if (val.is_uint64()) {
                time_delta = static_cast<int>(val.as_uint64());
            } else {
                return send(MakeErrorResponse(http::status::bad_request, "badRequest", "Failed to parse tick request JSON", req));
            }
        } catch (...) {
            return send(MakeErrorResponse(http::status::bad_request, "badRequest", "Failed to parse tick request JSON", req));
        }

        app_.Tick(std::chrono::milliseconds(time_delta));

        json::object response_obj;
        send(MakeJsonResponse(http::status::ok, json::serialize(response_obj), req));
    }

    template <typename Request, typename Send>
    void HandleGetMaps(Request&& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req, "GET, HEAD"));
        }
        
        json::array maps_array;
        for (const auto& map : app_.GetGame().GetMaps()) {
            json::object map_json;
            map_json["id"] = *map.GetId();
            map_json["name"] = map.GetName();
            maps_array.push_back(map_json);
        }
        send(MakeJsonResponse(http::status::ok, json::serialize(maps_array), req));
    }

    template <typename Request, typename Send>
    void HandleGetMap(Request&& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req, "GET, HEAD"));
        }
        
        std::string map_id = std::string(req.target().substr(13));
        const auto* map = app_.GetGame().FindMap(model::Map::Id{map_id});
        if (!map) {
            return send(MakeErrorResponse(http::status::not_found, "mapNotFound", "Map not found", req));
        }

        json::object map_json;
        map_json["id"] = *map->GetId();
        map_json["name"] = map->GetName();
        
        map_json["roads"] = json::array{};
        for (const auto& road : map->GetRoads()) {
            json::object road_json;
            if (road.IsHorizontal()) {
                road_json["x0"] = road.GetStart().x;
                road_json["y0"] = road.GetStart().y;
                road_json["x1"] = road.GetEnd().x;
            } else {
                road_json["x0"] = road.GetStart().x;
                road_json["y0"] = road.GetStart().y;
                road_json["y1"] = road.GetEnd().y;
            }
            map_json["roads"].as_array().push_back(road_json);
        }

        map_json["buildings"] = json::array{};
        for (const auto& building : map->GetBuildings()) {
            map_json["buildings"].as_array().push_back({
                {"x", building.GetBounds().position.x},
                {"y", building.GetBounds().position.y},
                {"w", building.GetBounds().size.width},
                {"h", building.GetBounds().size.height}
            });
        }

        map_json["offices"] = json::array{};
        for (const auto& office : map->GetOffices()) {
            map_json["offices"].as_array().push_back({
                {"id", *office.GetId()},
                {"x", office.GetPosition().x},
                {"y", office.GetPosition().y},
                {"offsetX", office.GetOffset().dx},
                {"offsetY", office.GetOffset().dy}
            });
        }

        send(MakeJsonResponse(http::status::ok, json::serialize(map_json), req));
    }
};

} // namespace http_handler
