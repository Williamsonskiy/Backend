#pragma once
#include "app.h"
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <string_view>
#include <string>
#include <stdexcept>

namespace http_handler {

namespace http = boost::beast::http;
namespace json = boost::json;

inline boost::beast::string_view ToBoostStr(std::string_view s) {
    return {s.data(), s.size()};
}
inline std::string_view ToStdStr(boost::beast::string_view s) {
    return {s.data(), s.size()};
}

namespace Endpoint {
    constexpr std::string_view GAME_JOIN = "/api/v1/game/join";
    constexpr std::string_view GAME_PLAYERS = "/api/v1/game/players";
    constexpr std::string_view GAME_STATE = "/api/v1/game/state";
    constexpr std::string_view GAME_ACTION = "/api/v1/game/player/action";
    constexpr std::string_view GAME_TICK = "/api/v1/game/tick";
    constexpr std::string_view MAPS = "/api/v1/maps";
    constexpr std::string_view MAPS_PREFIX = "/api/v1/maps/";
}

namespace JsonKey {
    constexpr const char* CODE = "code";
    constexpr const char* MESSAGE = "message";
    constexpr const char* USER_NAME = "userName";
    constexpr const char* MAP_ID = "mapId";
    constexpr const char* AUTH_TOKEN = "authToken";
    constexpr const char* PLAYER_ID = "playerId";
    constexpr const char* NAME = "name";
    constexpr const char* POS = "pos";
    constexpr const char* SPEED = "speed";
    constexpr const char* DIR = "dir";
    constexpr const char* PLAYERS = "players";
    constexpr const char* MOVE = "move";
    constexpr const char* TIME_DELTA = "timeDelta";
    constexpr const char* ID = "id";
    constexpr const char* ROADS = "roads";
    constexpr const char* BUILDINGS = "buildings";
    constexpr const char* OFFICES = "offices";
    constexpr const char* X0 = "x0";
    constexpr const char* Y0 = "y0";
    constexpr const char* X1 = "x1";
    constexpr const char* Y1 = "y1";
    constexpr const char* X = "x";
    constexpr const char* Y = "y";
    constexpr const char* W = "w";
    constexpr const char* H = "h";
    constexpr const char* OFFSET_X = "offsetX";
    constexpr const char* OFFSET_Y = "offsetY";
}

class ApiHandler {
public:
    explicit ApiHandler(app::App& app) : app_(app) {}

    template <typename Body, typename Allocator, typename Send>
    void HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string_view target = ToStdStr(req.target());

        if (target == Endpoint::GAME_JOIN) {
            return HandleJoinGame(std::move(req), std::forward<Send>(send));
        } else if (target == Endpoint::GAME_PLAYERS) {
            return HandleGetPlayers(std::move(req), std::forward<Send>(send));
        } else if (target == Endpoint::GAME_STATE) {
            return HandleGetGameState(std::move(req), std::forward<Send>(send));
        } else if (target == Endpoint::GAME_ACTION) {
            return HandlePlayerAction(std::move(req), std::forward<Send>(send));
        } else if (target == Endpoint::GAME_TICK) {
            if (app_.IsAutoTick()) {
                return send(MakeErrorResponse(http::status::bad_request, "badRequest", "Invalid endpoint", req));
            } else {
                return HandleGameTick(std::move(req), std::forward<Send>(send));
            }
        } else if (target == Endpoint::MAPS) {
            return HandleGetMaps(std::move(req), std::forward<Send>(send));
        } else if (target.starts_with(Endpoint::MAPS_PREFIX)) {
            return HandleGetMap(std::move(req), std::forward<Send>(send));
        }
        
        return send(MakeErrorResponse(http::status::bad_request, "badRequest", "Bad request", req));
    }

private:
    app::App& app_;

    template <typename Request>
    auto MakeErrorResponse(http::status status, std::string_view code, std::string_view message, const Request& req, std::string_view allow_methods = "") {
        json::object obj = {{JsonKey::CODE, code}, {JsonKey::MESSAGE, message}};
        auto res = MakeJsonResponse(status, json::serialize(obj), req);
        if (!allow_methods.empty()) {
            res.set(http::field::allow, ToBoostStr(allow_methods));
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
            if (!obj.contains(JsonKey::USER_NAME) || !obj.contains(JsonKey::MAP_ID)) {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Join game request parse error", req));
            }
            user_name = json::value_to<std::string>(obj.at(JsonKey::USER_NAME));
            map_id_str = json::value_to<std::string>(obj.at(JsonKey::MAP_ID));
        } catch (const std::exception& e) {
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
            {JsonKey::AUTH_TOKEN, token},
            {JsonKey::PLAYER_ID, player_id}
        };

        send(MakeJsonResponse(http::status::ok, json::serialize(response_obj), req));
    }

    template <typename Request, typename Send>
    void HandleGetPlayers(Request&& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req, "GET, HEAD"));
        }

        auto auth_it = req.find(http::field::authorization);
        if (auth_it == req.end()) {
            return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is missing", req));
        }
        std::string_view auth_val = ToStdStr(auth_it->value());
        if (!auth_val.starts_with("Bearer ") || auth_val.size() != 39) {
            return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is missing", req));
        }

        std::string token_str(auth_val.substr(7));
        app::Player* player = app_.GetPlayerByToken(token_str);
        if (!player) {
            return send(MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", req));
        }

        json::object players_obj;
        for (const auto& dog : player->GetSession()->GetDogs()) {
            players_obj[std::to_string(dog.GetId())] = json::object{{JsonKey::NAME, dog.GetName()}};
        }

        auto res = MakeJsonResponse(http::status::ok, json::serialize(players_obj), req);
        send(std::move(res));
    }

    template <typename Request, typename Send>
    void HandleGetGameState(Request&& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req, "GET, HEAD"));
        }

        auto auth_it = req.find(http::field::authorization);
        if (auth_it == req.end()) {
            return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is required", req));
        }
        std::string_view auth_val = ToStdStr(auth_it->value());
        if (!auth_val.starts_with("Bearer ") || auth_val.size() != 39) {
            return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is required", req));
        }

        std::string token_str(auth_val.substr(7));
        app::Player* player = app_.GetPlayerByToken(token_str);
        if (!player) {
            return send(MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", req));
        }

        json::object players_obj;
        for (const auto& dog : player->GetSession()->GetDogs()) {
            json::object dog_obj;
            dog_obj[JsonKey::POS] = json::array{dog.GetPosition().x, dog.GetPosition().y};
            dog_obj[JsonKey::SPEED] = json::array{dog.GetSpeed().ux, dog.GetSpeed().uy};
            dog_obj[JsonKey::DIR] = std::string(model::DirectionToString(dog.GetDirection()));
            players_obj[std::to_string(dog.GetId())] = dog_obj;
        }

        json::object root;
        root[JsonKey::PLAYERS] = players_obj;

        auto res = MakeJsonResponse(http::status::ok, json::serialize(root), req);
        send(std::move(res));
    }

    template <typename Request, typename Send>
    void HandlePlayerAction(Request&& req, Send&& send) {
        if (req.method() != http::verb::post) {
            return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req, "POST"));
        }

        auto ct_it = req.find(http::field::content_type);
        if (ct_it == req.end() || ToStdStr(ct_it->value()) != "application/json") {
            return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid content type", req));
        }

        auto auth_it = req.find(http::field::authorization);
        if (auth_it == req.end()) {
            return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is required", req));
        }
        std::string_view auth_val = ToStdStr(auth_it->value());
        if (!auth_val.starts_with("Bearer ") || auth_val.size() != 39) {
            return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is required", req));
        }

        std::string token_str(auth_val.substr(7));
        app::Player* player = app_.GetPlayerByToken(token_str);
        if (!player) {
            return send(MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", req));
        }

        std::string move_cmd;
        try {
            json::value jv = json::parse(req.body());
            if (!jv.is_object() || !jv.as_object().contains(JsonKey::MOVE)) {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", req));
            }
            move_cmd = json::value_to<std::string>(jv.as_object().at(JsonKey::MOVE));
            if (move_cmd != "U" && move_cmd != "D" && move_cmd != "L" && move_cmd != "R" && move_cmd != "") {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse action", req));
            }
        } catch (const std::exception& e) {
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
        
        auto ct_it = req.find(http::field::content_type);
        if (ct_it == req.end() || ToStdStr(ct_it->value()) != "application/json") {
            return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid content type", req));
        }

        int time_delta = 0;
        try {
            json::value jv = json::parse(req.body());
            if (!jv.is_object() || !jv.as_object().contains(JsonKey::TIME_DELTA)) {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse tick request JSON", req));
            }
            
            const auto& val = jv.as_object().at(JsonKey::TIME_DELTA);
            if (val.is_int64()) {
                time_delta = static_cast<int>(val.as_int64());
            } else if (val.is_uint64()) {
                time_delta = static_cast<int>(val.as_uint64());
            } else {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse tick request JSON", req));
            }
        } catch (const std::exception& e) {
            return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse tick request JSON", req));
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
            map_json[JsonKey::ID] = *map.GetId();
            map_json[JsonKey::NAME] = map.GetName();
            maps_array.push_back(map_json);
        }
        send(MakeJsonResponse(http::status::ok, json::serialize(maps_array), req));
    }

    template <typename Request, typename Send>
    void HandleGetMap(Request&& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req, "GET, HEAD"));
        }
        
        std::string map_id = std::string(ToStdStr(req.target()).substr(Endpoint::MAPS_PREFIX.size()));
        const auto* map = app_.GetGame().FindMap(model::Map::Id{map_id});
        if (!map) {
            return send(MakeErrorResponse(http::status::not_found, "mapNotFound", "Map not found", req));
        }

        json::object map_json;
        map_json[JsonKey::ID] = *map->GetId();
        map_json[JsonKey::NAME] = map->GetName();
        
        map_json[JsonKey::ROADS] = json::array{};
        for (const auto& road : map->GetRoads()) {
            json::object road_json;
            if (road.IsHorizontal()) {
                road_json[JsonKey::X0] = road.GetStart().x;
                road_json[JsonKey::Y0] = road.GetStart().y;
                road_json[JsonKey::X1] = road.GetEnd().x;
            } else {
                road_json[JsonKey::X0] = road.GetStart().x;
                road_json[JsonKey::Y0] = road.GetStart().y;
                road_json[JsonKey::Y1] = road.GetEnd().y;
            }
            map_json[JsonKey::ROADS].as_array().push_back(road_json);
        }

        map_json[JsonKey::BUILDINGS] = json::array{};
        for (const auto& building : map->GetBuildings()) {
            map_json[JsonKey::BUILDINGS].as_array().push_back({
                {JsonKey::X, building.GetBounds().position.x},
                {JsonKey::Y, building.GetBounds().position.y},
                {JsonKey::W, building.GetBounds().size.width},
                {JsonKey::H, building.GetBounds().size.height}
            });
        }

        map_json[JsonKey::OFFICES] = json::array{};
        for (const auto& office : map->GetOffices()) {
            map_json[JsonKey::OFFICES].as_array().push_back({
                {JsonKey::ID, *office.GetId()},
                {JsonKey::X, office.GetPosition().x},
                {JsonKey::Y, office.GetPosition().y},
                {JsonKey::OFFSET_X, office.GetOffset().dx},
                {JsonKey::OFFSET_Y, office.GetOffset().dy}
            });
        }

        send(MakeJsonResponse(http::status::ok, json::serialize(map_json), req));
    }
};

} // namespace http_handler
