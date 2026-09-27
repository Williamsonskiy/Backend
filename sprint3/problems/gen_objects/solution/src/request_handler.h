#pragma once

#include "http_server.h"
#include "model.h"
#include "players.h"
#include "player_tokens.h"
#include "use_cases.h"
#include "extra_data.h"

#include <boost/json.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

using namespace std::literals;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game,
                            const extra_data::ExtraData& extra_data,
                            std::filesystem::path static_root,
                            bool manual_tick_enabled);

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        const std::string target = std::string(req.target());
        const unsigned version = req.version();
        const bool keep_alive = req.keep_alive();

        if (target == kJoinGameEndpoint) {
            return send(HandleJoinGame(req));
        }

        if (target == kPlayersEndpoint) {
            return send(HandlePlayers(req));
        }

        if (target == kGameStateEndpoint) {
            return send(HandleGameState(req));
        }

        if (target == kPlayerActionEndpoint) {
            return send(HandlePlayerAction(req));
        }

        if (target == kGameTickEndpoint) {
            return send(HandleGameTick(req));
        }

        if (target == kMapsEndpoint || target.starts_with(kMapsEndpointPrefix)) {
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                auto response = MakeErrorResponse(
                    version,
                    keep_alive,
                    http::status::method_not_allowed,
                    "invalidMethod"sv,
                    "Invalid method"sv
                );
                response.set(http::field::allow, "GET, HEAD");
                return send(std::move(response));
            }

            if (target == kMapsEndpoint) {
                auto response = MakeJsonResponse(version, keep_alive, SerializeMaps());

                if (req.method() == http::verb::head) {
                    response.body().clear();
                    response.content_length(0);
                }

                return send(std::move(response));
            }

            const std::string map_id = target.substr(kMapsEndpointPrefix.size());

            if (map_id.empty()) {
                return send(MakeBadRequest(version, keep_alive, kBadRequestCode, kBadRequestMessage));
            }

            if (const model::Map* map = game_.FindMap(model::Map::Id(map_id))) {
                auto response = MakeJsonResponse(version, keep_alive, SerializeMap(*map));

                if (req.method() == http::verb::head) {
                    response.body().clear();
                    response.content_length(0);
                }

                return send(std::move(response));
            }

            auto response = MakeNotFound(version, keep_alive, kMapNotFoundCode, kMapNotFoundMessage);

            if (req.method() == http::verb::head) {
                response.body().clear();
                response.content_length(0);
            }

            return send(std::move(response));
        }

        if (target.starts_with(kApiPrefix)) {
            return send(MakeBadRequest(version, keep_alive, kBadRequestCode, kBadRequestMessage));
        }

        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return send(MakeBadRequest(version, keep_alive, kBadRequestCode, kBadRequestMessage));
        }

        return send(HandleStaticFile(req, version, keep_alive));
    }

private:
    using StringRequest = http::request<http::string_body>;
    using StringResponse = http::response<http::string_body>;

    static constexpr std::string_view kApiPrefix = "/api/"sv;
    static constexpr std::string_view kMapsEndpoint = "/api/v1/maps"sv;
    static constexpr std::string_view kMapsEndpointPrefix = "/api/v1/maps/"sv;

    static constexpr std::string_view kJoinGameEndpoint = "/api/v1/game/join"sv;
    static constexpr std::string_view kPlayersEndpoint = "/api/v1/game/players"sv;
    static constexpr std::string_view kGameStateEndpoint = "/api/v1/game/state"sv;
    static constexpr std::string_view kPlayerActionEndpoint = "/api/v1/game/player/action"sv;
    static constexpr std::string_view kGameTickEndpoint = "/api/v1/game/tick"sv;

    static constexpr std::string_view kBadRequestCode = "badRequest"sv;
    static constexpr std::string_view kBadRequestMessage = "Bad request"sv;
    static constexpr std::string_view kInvalidEndpointMessage = "Invalid endpoint"sv;
    static constexpr std::string_view kMapNotFoundCode = "mapNotFound"sv;
    static constexpr std::string_view kMapNotFoundMessage = "Map not found"sv;

    StringResponse MakeJsonResponse(unsigned version,
                                    bool keep_alive,
                                    json::value body,
                                    http::status status = http::status::ok) const;

    StringResponse MakeErrorResponse(unsigned version,
                                     bool keep_alive,
                                     http::status status,
                                     std::string_view code,
                                     std::string_view message) const;

    StringResponse MakeBadRequest(unsigned version,
                                  bool keep_alive,
                                  std::string_view code,
                                  std::string_view message) const;

    StringResponse MakeNotFound(unsigned version,
                                bool keep_alive,
                                std::string_view code,
                                std::string_view message) const;

    StringResponse HandleStaticFile(const http::request<http::string_body>& req,
                                    unsigned version,
                                    bool keep_alive) const;

    StringResponse HandleJoinGame(const StringRequest& req);
    StringResponse HandlePlayers(const StringRequest& req);
    StringResponse HandleGameState(const StringRequest& req);
    StringResponse HandlePlayerAction(const StringRequest& req);
    StringResponse HandleGameTick(const StringRequest& req);

    json::value SerializeMaps() const;
    json::value SerializeMap(const model::Map& map) const;

private:
    model::Game& game_;
    const extra_data::ExtraData& extra_data_;
    std::filesystem::path static_root_;
    bool manual_tick_enabled_ = true;

    app::Players players_;
    app::PlayerTokens player_tokens_;
};

}  // namespace http_handler
