#pragma once
#include "app.h"
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <string_view>

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
        res.body() = body;
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
            user_name = jv.as_object().at("userName").as_string().c_str();
            map_id_str = jv.as_object().at("mapId").as_string().c_str();
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

        std::string_view auth_header = auth_it->value();
        if (!auth_header.starts_with("Bearer ") || auth_header.size() != 39) {
            return send(MakeErrorResponse(http::status::unauthorized, "invalidToken", "Authorization header is missing", req));
        }

        std::string token_str(auth_header.substr(7));
        app::Player* player = app_.GetPlayerByToken(token_str);
        if (!player) {
            return send(MakeErrorResponse(http::status::unauthorized, "unknownToken", "Player token has not been found", req));
        }

        json::object players_obj;
        for (const auto& dog : player->GetSession()->GetDogs()) {
            players_obj[std::to_string(dog.GetId())] = {{"name", dog.GetName()}};
        }

        send(MakeJsonResponse(http::status::ok, json::serialize(players_obj), req));
    }

    template <typename Request, typename Send>
    void HandleGetMaps(Request&& req, Send&& send) {
        // [ВСТАВЬТЕ СЮДА] Вашу логику выдачи списка карт из 1 спринта.
        // Пример (псевдокод): send(MakeJsonResponse(http::status::ok, json_loader::GetMapsJson(...), req));
    }

    template <typename Request, typename Send>
    void HandleGetMap(Request&& req, Send&& send) {
        // [ВСТАВЬТЕ СЮДА] Вашу логику поиска и выдачи конкретной карты из 1 спринта.
    }
};

} // namespace http_handler
