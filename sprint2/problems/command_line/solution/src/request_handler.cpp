#include "request_handler.h"

#include "request_handler.h"
#include "json_serializer.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace http_handler {

namespace fs = std::filesystem;

namespace {

constexpr beast::string_view APPLICATION_JSON = "application/json";
constexpr beast::string_view CACHE_CONTROL_NO_CACHE = "no-cache";

std::string MakeErrorBody(std::string_view code, std::string_view message) {
    json::object obj;
    obj["code"] = std::string(code);
    obj["message"] = std::string(message);
    return json::serialize(obj);
}

bool IsJsonContentType(beast::string_view value) {
    return std::string(value).starts_with("application/json");
}

std::optional<std::string> ExtractBearerToken(const http::request<http::string_body>& req) {
    auto auth_it = req.find(http::field::authorization);

    if (auth_it == req.end()) {
        return std::nullopt;
    }

    std::string value = std::string(auth_it->value());
    const std::string prefix = "Bearer ";

    if (!value.starts_with(prefix)) {
        return std::nullopt;
    }

    std::string token = value.substr(prefix.size());

    if (token.size() != 32) {
        return std::nullopt;
    }

    return token;
}

std::string GetMimeType(const fs::path& path) {
    static const std::unordered_map<std::string, std::string> types = {
        {".html", "text/html"},
        {".htm", "text/html"},
        {".css", "text/css"},
        {".js", "text/javascript"},
        {".json", "application/json"},
        {".png", "image/png"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif", "image/gif"},
        {".bmp", "image/bmp"},
        {".ico", "image/vnd.microsoft.icon"},
        {".svg", "image/svg+xml"},
        {".svgz", "image/svg+xml"},
        {".mp3", "audio/mpeg"},
        {".txt", "text/plain"},
        {".fbx", "application/octet-stream"}
    };

    std::string ext = path.extension().string();

    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (const auto it = types.find(ext); it != types.end()) {
        return it->second;
    }

    return "application/octet-stream";
}

std::string ReadFile(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);

    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

bool IsSubPath(const fs::path& path, const fs::path& base) {
    auto canonical_path = fs::weakly_canonical(path);
    auto canonical_base = fs::weakly_canonical(base);

    auto path_it = canonical_path.begin();
    auto base_it = canonical_base.begin();

    for (; base_it != canonical_base.end(); ++base_it, ++path_it) {
        if (path_it == canonical_path.end() || *path_it != *base_it) {
            return false;
        }
    }

    return true;
}

}  // namespace

RequestHandler::RequestHandler(model::Game& game,
                               std::filesystem::path static_root,
                               bool manual_tick_enabled)
    : game_{game}
    , static_root_{std::move(static_root)}
    , manual_tick_enabled_{manual_tick_enabled} {
}

RequestHandler::StringResponse RequestHandler::MakeJsonResponse(
    unsigned version,
    bool keep_alive,
    json::value body,
    http::status status) const {
    StringResponse response(status, version);
    response.set(http::field::content_type, "application/json");
    response.set(http::field::cache_control, "no-cache");
    response.body() = json::serialize(body);
    response.content_length(response.body().size());
    response.keep_alive(keep_alive);
    return response;
}

RequestHandler::StringResponse RequestHandler::MakeErrorResponse(
    unsigned version,
    bool keep_alive,
    http::status status,
    std::string_view code,
    std::string_view message) const {
    json::object obj;
    obj["code"] = std::string(code);
    obj["message"] = std::string(message);
    return MakeJsonResponse(version, keep_alive, obj, status);
}

RequestHandler::StringResponse RequestHandler::MakeBadRequest(
    unsigned version,
    bool keep_alive,
    std::string_view code,
    std::string_view message) const {
    return MakeErrorResponse(version, keep_alive, http::status::bad_request, code, message);
}

RequestHandler::StringResponse RequestHandler::MakeNotFound(
    unsigned version,
    bool keep_alive,
    std::string_view code,
    std::string_view message) const {
    return MakeErrorResponse(version, keep_alive, http::status::not_found, code, message);
}

RequestHandler::StringResponse RequestHandler::HandleStaticFile(
    const http::request<http::string_body>& req,
    unsigned version,
    bool keep_alive) const {
    std::string target = std::string(req.target());

    fs::path requested_path;

    if (target == "/") {
        requested_path = static_root_ / "index.html";
    } else {
        requested_path = static_root_ / target.substr(1);
    }

    if (!IsSubPath(requested_path, static_root_)) {
        return MakeBadRequest(version, keep_alive, kBadRequestCode, kBadRequestMessage);
    }

    if (fs::is_directory(requested_path)) {
        requested_path /= "index.html";
    }

    if (!fs::exists(requested_path) || !fs::is_regular_file(requested_path)) {
        StringResponse response(http::status::not_found, version);
        response.set(http::field::content_type, "text/plain");
        response.body() = "File not found";
        response.content_length(response.body().size());
        response.keep_alive(keep_alive);
        return response;
    }

    std::string body = ReadFile(requested_path);

    StringResponse response(http::status::ok, version);
    response.set(http::field::content_type, GetMimeType(requested_path));
    response.body() = std::move(body);
    response.content_length(response.body().size());
    response.keep_alive(keep_alive);

    return response;
}

RequestHandler::StringResponse RequestHandler::HandleJoinGame(const StringRequest& req) {
    const auto version = req.version();
    const bool keep_alive = req.keep_alive();

    if (req.method() != http::verb::post) {
        auto body = MakeErrorBody("invalidMethod", "Only POST method is expected");

        StringResponse response{http::status::method_not_allowed, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.set(http::field::allow, "POST");
        response.keep_alive(keep_alive);
        response.body() = std::move(body);
        response.prepare_payload();

        return response;
    }

    auto content_type_it = req.find(http::field::content_type);
    if (content_type_it == req.end() || !IsJsonContentType(content_type_it->value())) {
        auto body = MakeErrorBody("invalidArgument", "Invalid content type");

        StringResponse response{http::status::bad_request, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.keep_alive(keep_alive);
        response.body() = std::move(body);
        response.prepare_payload();

        return response;
    }

    try {
        json::value value = json::parse(req.body());

        if (!value.is_object()) {
            throw std::invalid_argument("Body is not object");
        }

        const json::object& obj = value.as_object();

        if (!obj.contains("userName") || !obj.at("userName").is_string()
            || !obj.contains("mapId") || !obj.at("mapId").is_string()) {
            throw std::invalid_argument("Invalid join fields");
        }

        std::string user_name = std::string(obj.at("userName").as_string());
        std::string map_id = std::string(obj.at("mapId").as_string());

        if (user_name.empty()) {
            auto body = MakeErrorBody("invalidArgument", "Invalid name");

            StringResponse response{http::status::bad_request, version};
            response.set(http::field::content_type, APPLICATION_JSON);
            response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
            response.keep_alive(keep_alive);
            response.body() = std::move(body);
            response.prepare_payload();

            return response;
        }

        const model::Map* map = game_.FindMap(model::Map::Id{map_id});

        if (map == nullptr) {
            auto body = MakeErrorBody("mapNotFound", "Map not found");

            StringResponse response{http::status::not_found, version};
            response.set(http::field::content_type, APPLICATION_JSON);
            response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
            response.keep_alive(keep_alive);
            response.body() = std::move(body);
            response.prepare_payload();

            return response;
        }

        model::GameSession& session = game_.FindOrCreateSession(*map);

        app::JoinGameUseCase join_use_case{players_, player_tokens_};
        app::JoinGameUseCase::Result result = join_use_case.Join(session, std::move(user_name));

        json::object response_obj;
        response_obj["authToken"] = result.auth_token;
        response_obj["playerId"] = result.player_id;

        StringResponse response{http::status::ok, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.keep_alive(keep_alive);
        response.body() = json::serialize(response_obj);
        response.prepare_payload();

        return response;

    } catch (...) {
        auto body = MakeErrorBody("invalidArgument", "Join game request parse error");

        StringResponse response{http::status::bad_request, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.keep_alive(keep_alive);
        response.body() = std::move(body);
        response.prepare_payload();

        return response;
    }
}

RequestHandler::StringResponse RequestHandler::HandlePlayers(const StringRequest& req) {
    const auto version = req.version();
    const bool keep_alive = req.keep_alive();

    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        auto body = MakeErrorBody("invalidMethod", "Invalid method");

        StringResponse response{http::status::method_not_allowed, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.set(http::field::allow, "GET, HEAD");
        response.keep_alive(keep_alive);
        response.body() = std::move(body);
        response.prepare_payload();

        return response;
    }

    std::optional<std::string> token = ExtractBearerToken(req);

    if (!token.has_value()) {
        auto body = MakeErrorBody("invalidToken", "Authorization header is missing");

        StringResponse response{http::status::unauthorized, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.keep_alive(keep_alive);
        response.body() = std::move(body);
        response.prepare_payload();

        if (req.method() == http::verb::head) {
            response.body().clear();
        }

        return response;
    }

    app::Player* player = player_tokens_.FindPlayerByToken(*token);

    if (player == nullptr) {
        auto body = MakeErrorBody("unknownToken", "Player token has not been found");

        StringResponse response{http::status::unauthorized, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.keep_alive(keep_alive);
        response.body() = std::move(body);
        response.prepare_payload();

        if (req.method() == http::verb::head) {
            response.body().clear();
        }

        return response;
    }

    json::object players_json;

    for (app::Player* session_player : players_.FindBySession(player->GetSession())) {
        json::object player_json;
        player_json["name"] = session_player->GetName();

        players_json[std::to_string(session_player->GetId())] = std::move(player_json);
    }

    std::string body = json::serialize(players_json);

    StringResponse response{http::status::ok, version};
    response.set(http::field::content_type, APPLICATION_JSON);
    response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
    response.keep_alive(keep_alive);
    response.body() = std::move(body);
    response.prepare_payload();

    if (req.method() == http::verb::head) {
        response.body().clear();
    }

    return response;
}

RequestHandler::StringResponse RequestHandler::HandleGameState(const StringRequest& req) {
    const auto version = req.version();
    const bool keep_alive = req.keep_alive();

    if (req.method() != http::verb::get && req.method() != http::verb::head) {
        auto body = MakeErrorBody("invalidMethod", "Invalid method");

        StringResponse response{http::status::method_not_allowed, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.set(http::field::allow, "GET, HEAD");
        response.keep_alive(keep_alive);
        response.body() = std::move(body);
        response.prepare_payload();

        return response;
    }

    std::optional<std::string> token = ExtractBearerToken(req);

    if (!token.has_value()) {
        auto body = MakeErrorBody("invalidToken", "Authorization header is required");

        StringResponse response{http::status::unauthorized, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.keep_alive(keep_alive);
        response.body() = std::move(body);
        response.prepare_payload();

        if (req.method() == http::verb::head) {
            response.body().clear();
        }

        return response;
    }

    app::Player* player = player_tokens_.FindPlayerByToken(*token);

    if (player == nullptr) {
        auto body = MakeErrorBody("unknownToken", "Player token has not been found");

        StringResponse response{http::status::unauthorized, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.keep_alive(keep_alive);
        response.body() = std::move(body);
        response.prepare_payload();

        if (req.method() == http::verb::head) {
            response.body().clear();
        }

        return response;
    }

    json::object players_json;

    for (app::Player* session_player : players_.FindBySession(player->GetSession())) {
        const auto dog_id = session_player->GetDogId();
        const auto& dogs = session_player->GetSession().GetDogs();

        auto dog_it = dogs.find(dog_id);
        if (dog_it == dogs.end()) {
            continue;
        }

        const model::Dog& dog = dog_it->second;

        const model::Position pos = dog.GetPosition();
        const model::Speed speed = dog.GetSpeed();

        json::array pos_json;
        pos_json.emplace_back(pos.x);
        pos_json.emplace_back(pos.y);

        json::array speed_json;
        speed_json.emplace_back(speed.dx);
        speed_json.emplace_back(speed.dy);

        json::object player_json;
        player_json["pos"] = std::move(pos_json);
        player_json["speed"] = std::move(speed_json);
        player_json["dir"] = model::DirectionToString(dog.GetDirection());

        players_json[std::to_string(session_player->GetId())] = std::move(player_json);
    }

    json::object result;
    result["players"] = std::move(players_json);

    std::string body = json::serialize(result);

    StringResponse response{http::status::ok, version};
    response.set(http::field::content_type, APPLICATION_JSON);
    response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
    response.keep_alive(keep_alive);
    response.body() = std::move(body);
    response.prepare_payload();

    if (req.method() == http::verb::head) {
        response.body().clear();
    }

    return response;
}

RequestHandler::StringResponse RequestHandler::HandlePlayerAction(const StringRequest& req) {
    const auto version = req.version();
    const bool keep_alive = req.keep_alive();

    if (req.method() != http::verb::post) {
        auto body = MakeErrorBody("invalidMethod", "Invalid method");

        StringResponse response{http::status::method_not_allowed, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.set(http::field::allow, "POST");
        response.keep_alive(keep_alive);
        response.body() = std::move(body);
        response.prepare_payload();

        return response;
    }

    auto content_type_it = req.find(http::field::content_type);
    if (content_type_it == req.end() || !IsJsonContentType(content_type_it->value())) {
        auto body = MakeErrorBody("invalidArgument", "Invalid content type");

        StringResponse response{http::status::bad_request, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.keep_alive(keep_alive);
        response.body() = std::move(body);
        response.prepare_payload();

        return response;
    }

    std::optional<std::string> token = ExtractBearerToken(req);

    if (!token.has_value()) {
        auto body = MakeErrorBody("invalidToken", "Authorization header is required");

        StringResponse response{http::status::unauthorized, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.keep_alive(keep_alive);
        response.body() = std::move(body);
        response.prepare_payload();

        return response;
    }

    app::Player* player = player_tokens_.FindPlayerByToken(*token);

    if (player == nullptr) {
        auto body = MakeErrorBody("unknownToken", "Player token has not been found");

        StringResponse response{http::status::unauthorized, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.keep_alive(keep_alive);
        response.body() = std::move(body);
        response.prepare_payload();

        return response;
    }

    try {
        json::value value = json::parse(req.body());

        if (!value.is_object()) {
            throw std::invalid_argument("Action is not object");
        }

        const json::object& obj = value.as_object();

        if (!obj.contains("move") || !obj.at("move").is_string()) {
            throw std::invalid_argument("Move is missing");
        }

        std::string move = std::string(obj.at("move").as_string());

        if (move != "L" && move != "R" && move != "U" && move != "D" && move != "") {
            throw std::invalid_argument("Invalid move");
        }

        model::GameSession& session = player->GetSession();
        model::Dog* dog = session.FindDog(player->GetDogId());

        if (dog == nullptr) {
            throw std::invalid_argument("Dog not found");
        }

        const double speed = session.GetMap().GetDogSpeed();

        if (move == "L") {
            dog->SetDirection(model::Direction::WEST);
            dog->SetSpeed({-speed, 0.0});
        } else if (move == "R") {
            dog->SetDirection(model::Direction::EAST);
            dog->SetSpeed({speed, 0.0});
        } else if (move == "U") {
            dog->SetDirection(model::Direction::NORTH);
            dog->SetSpeed({0.0, -speed});
        } else if (move == "D") {
            dog->SetDirection(model::Direction::SOUTH);
            dog->SetSpeed({0.0, speed});
        } else {
            dog->Stop();
        }

        StringResponse response{http::status::ok, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.keep_alive(keep_alive);
        response.body() = "{}";
        response.prepare_payload();

        return response;

    } catch (...) {
        auto body = MakeErrorBody("invalidArgument", "Failed to parse action");

        StringResponse response{http::status::bad_request, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.keep_alive(keep_alive);
        response.body() = std::move(body);
        response.prepare_payload();

        return response;
    }
}

RequestHandler::StringResponse RequestHandler::HandleGameTick(const StringRequest& req) {
    const auto version = req.version();
    const bool keep_alive = req.keep_alive();

    if (!manual_tick_enabled_) {
        return MakeBadRequest(version, keep_alive, kBadRequestCode, kInvalidEndpointMessage);
    }

    if (req.method() != http::verb::post) {
        auto body = MakeErrorBody("invalidMethod", "Invalid method");

        StringResponse response{http::status::method_not_allowed, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.set(http::field::allow, "POST");
        response.keep_alive(keep_alive);
        response.body() = std::move(body);
        response.prepare_payload();

        return response;
    }

    auto content_type_it = req.find(http::field::content_type);
    if (content_type_it == req.end() || !IsJsonContentType(content_type_it->value())) {
        auto body = MakeErrorBody("invalidArgument", "Invalid content type");

        StringResponse response{http::status::bad_request, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.keep_alive(keep_alive);
        response.body() = std::move(body);
        response.prepare_payload();

        return response;
    }

    try {
        json::value value = json::parse(req.body());

        if (!value.is_object()) {
            throw std::invalid_argument("Tick request is not object");
        }

        const json::object& obj = value.as_object();

        if (!obj.contains("timeDelta") || !obj.at("timeDelta").is_int64()) {
            throw std::invalid_argument("timeDelta is missing or invalid");
        }

        const std::int64_t time_delta_ms = obj.at("timeDelta").as_int64();

        if (time_delta_ms < 0) {
            throw std::invalid_argument("timeDelta is negative");
        }

        const double delta_seconds = static_cast<double>(time_delta_ms) / 1000.0;

        game_.Update(delta_seconds);

        StringResponse response{http::status::ok, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.keep_alive(keep_alive);
        response.body() = "{}";
        response.prepare_payload();

        return response;

    } catch (...) {
        auto body = MakeErrorBody("invalidArgument", "Failed to parse tick request JSON");

        StringResponse response{http::status::bad_request, version};
        response.set(http::field::content_type, APPLICATION_JSON);
        response.set(http::field::cache_control, CACHE_CONTROL_NO_CACHE);
        response.keep_alive(keep_alive);
        response.body() = std::move(body);
        response.prepare_payload();

        return response;
    }
}

json::value RequestHandler::SerializeMaps() const {
    return json_serializer::SerializeMaps(game_.GetMaps());
}

json::value RequestHandler::SerializeMap(const model::Map& map) const {
    return json_serializer::SerializeMap(map);
}

}  // namespace http_handler
