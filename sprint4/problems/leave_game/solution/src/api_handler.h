#pragma once
#include "app.h"
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <string_view>
#include <string>
#include <sstream>

namespace http_handler {

namespace http = boost::beast::http;
namespace json = boost::json;

inline boost::beast::string_view ToBoostStr(std::string_view s) {
    return {s.data(), s.size()};
}
inline std::string_view ToStdStr(boost::beast::string_view s) {
    return {s.data(), s.size()};
}

class ApiHandler {
public:
    explicit ApiHandler(app::App& app) : app_(app) {}

    template <typename Body, typename Allocator, typename Send>
    void HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string_view target = ToStdStr(req.target());

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
        } else if (target == "/api/v1/game/records" || target.starts_with("/api/v1/game/records?")) {
            return HandleGetRecords(std::move(req), std::forward<Send>(send));
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
        for (const auto& [id, dog] : player->GetSession()->GetDogs()) {
            players_obj[std::to_string(id)] = json::object{{"name", dog.GetName()}};
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

        auto* session = player->GetSession();
        
        json::object players_obj;
        for (const auto& [id, dog] : session->GetDogs()) {
            json::object dog_obj;
            dog_obj["pos"] = json::array{dog.GetPosition().x, dog.GetPosition().y};
            dog_obj["speed"] = json::array{dog.GetSpeed().ux, dog.GetSpeed().uy};
            dog_obj["dir"] = std::string(model::DirectionToString(dog.GetDirection()));
            
            json::array bag_arr;
            for (const auto& item : dog.GetBag()) {
                bag_arr.push_back(json::object{{"id", item.id}, {"type", item.type}});
            }
            dog_obj["bag"] = std::move(bag_arr);
            dog_obj["score"] = dog.GetScore();

            players_obj[std::to_string(id)] = dog_obj;
        }

        json::object lost_objects_obj;
        for (const auto& [id, lost_object] : session->GetLostObjects()) {
            json::object lo_obj;
            lo_obj["type"] = lost_object.type;
            lo_obj["pos"] = json::array{lost_object.pos.x, lost_object.pos.y};
            lost_objects_obj[std::to_string(id)] = lo_obj;
        }

        json::object root;
        root["players"] = players_obj;
        root["lostObjects"] = lost_objects_obj;

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
        
        auto ct_it = req.find(http::field::content_type);
        if (ct_it == req.end() || ToStdStr(ct_it->value()) != "application/json") {
            return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Invalid content type", req));
        }

        int time_delta = 0;
        try {
            json::value jv = json::parse(req.body());
            if (!jv.is_object() || !jv.as_object().contains("timeDelta")) {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse tick request JSON", req));
            }
            
            const auto& val = jv.as_object().at("timeDelta");
            if (val.is_int64()) {
                time_delta = static_cast<int>(val.as_int64());
            } else if (val.is_uint64()) {
                time_delta = static_cast<int>(val.as_uint64());
            } else {
                return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse tick request JSON", req));
            }
        } catch (...) {
            return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "Failed to parse tick request JSON", req));
        }

        app_.Tick(std::chrono::milliseconds(time_delta));

        json::object response_obj;
        send(MakeJsonResponse(http::status::ok, json::serialize(response_obj), req));
    }

    template <typename Request, typename Send>
    void HandleGetRecords(Request&& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return send(MakeErrorResponse(http::status::method_not_allowed, "invalidMethod", "Invalid method", req, "GET, HEAD"));
        }
        
        int start = 0;
        int maxItems = 100;
        
        std::string target = std::string(ToStdStr(req.target()));
        auto q_pos = target.find('?');
        if (q_pos != std::string::npos) {
            std::string query = target.substr(q_pos + 1);
            std::istringstream ss(query);
            std::string keyval;
            while (std::getline(ss, keyval, '&')) {
                auto eq = keyval.find('=');
                if (eq != std::string::npos) {
                    std::string key = keyval.substr(0, eq);
                    std::string val = keyval.substr(eq + 1);
                    if (key == "start") {
                        try { start = std::stoi(val); } catch (...) {}
                    } else if (key == "maxItems") {
                        try { maxItems = std::stoi(val); } catch (...) {}
                    }
                }
            }
        }
        
        if (maxItems > 100) {
            return send(MakeErrorResponse(http::status::bad_request, "invalidArgument", "maxItems can't be greater than 100", req));
        }
        
        std::vector<postgres::Record> records;
        if (app_.GetDatabase()) {
            records = app_.GetDatabase()->GetRecords(start, maxItems);
        }
        
        json::array arr;
        for (const auto& rec : records) {
            arr.push_back({
                {"name", rec.name},
                {"score", rec.score},
                {"playTime", rec.play_time}
            });
        }
        
        send(MakeJsonResponse(http::status::ok, json::serialize(arr), req));
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
        
        std::string map_id = std::string(ToStdStr(req.target()).substr(13));
        const auto* map = app_.GetGame().FindMap(model::Map::Id{map_id});
        if (!map) {
            return send(MakeErrorRe
