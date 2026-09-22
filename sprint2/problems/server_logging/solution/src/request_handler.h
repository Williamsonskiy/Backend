#pragma once

#include "model.h"
#include "logger.h"
#include <boost/json.hpp>
#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <filesystem>

namespace http = boost::beast::http;

namespace http_handler {

namespace fs = std::filesystem;

// Объявления функций
std::string urlDecode(const std::string& encodedString);
bool IsSubPath(fs::path path, fs::path base);
std::string getContentType(const fs::path& filePath);
void SetIdAndName(boost::json::object& json_map, const model::Map* map);
void SetRoads(boost::json::object& json_map, const model::Map* map);
void SetBuildings(boost::json::object& json_map, const model::Map* map);
void SetOffices(boost::json::object& json_map, const model::Map* map);

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, fs::path static_path)
        : game_{game}
        , static_path_{fs::weakly_canonical(std::move(static_path))} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        auto text_response = [&req](http::status status, std::string_view text, std::string_view content_type) {
            http::response<http::string_body> response(status, req.version());
            response.set(http::field::content_type, content_type);
            response.body() = std::string(text);
            response.content_length(text.size());
            response.keep_alive(req.keep_alive());
            return response;
        };

        std::string target(req.target());
        target = urlDecode(target);

        // Обрабатываем только GET и HEAD
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return send(text_response(http::status::method_not_allowed, "Invalid method", "text/plain"));
        }

        // --- Обработка API ---
        if (target.starts_with("/api/")) {
            if (target == "/api/v1/maps") {
                boost::json::array maps_array;
                for (const auto& map : game_.GetMaps()) {
                    boost::json::object map_json;
                    SetIdAndName(map_json, &map);
                    maps_array.push_back(map_json);
                }
                return send(text_response(http::status::ok, boost::json::serialize(maps_array), "application/json"));
            } else if (target.starts_with("/api/v1/maps/")) {
                std::string map_id = target.substr(13);
                const auto* map = game_.FindMap(model::Map::Id{map_id});
                if (!map) {
                    boost::json::object error = {{"code", "mapNotFound"}, {"message", "Map not found"}};
                    return send(text_response(http::status::not_found, boost::json::serialize(error), "application/json"));
                }
                boost::json::object map_json;
                SetIdAndName(map_json, map);
                SetRoads(map_json, map);
                SetBuildings(map_json, map);
                SetOffices(map_json, map);
                return send(text_response(http::status::ok, boost::json::serialize(map_json), "application/json"));
            } else {
                boost::json::object error = {{"code", "badRequest"}, {"message", "Bad request"}};
                return send(text_response(http::status::bad_request, boost::json::serialize(error), "application/json"));
            }
        }

        // --- Обработка статических файлов ---
        if (target == "/") {
            target = "/index.html";
        }

        fs::path file_path = fs::weakly_canonical(static_path_ / target.substr(1));
        if (!IsSubPath(file_path, static_path_)) {
            return send(text_response(http::status::bad_request, "Bad request", "text/plain"));
        }

        if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
            return send(text_response(http::status::not_found, "File not found", "text/plain"));
        }

        http::file_body::value_type file;
        if (boost::system::error_code ec; file.open(file_path.c_str(), boost::beast::file_mode::read, ec), ec) {
            return send(text_response(http::status::internal_server_error, "Failed to open file", "text/plain"));
        }

        http::response<http::file_body> response(http::status::ok, req.version());
        response.set(http::field::content_type, getContentType(file_path));
        response.body() = std::move(file);
        response.prepare_payload();
        response.keep_alive(req.keep_alive());
        send(std::move(response));
    }

private:
    model::Game& game_;
    fs::path static_path_;
};

} // namespace http_handler

template <typename SomeRequestHandler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(SomeRequestHandler&& decorated)
        : decorated_(std::move(decorated)) {}

    template <typename Body, typename Allocator, typename Send>
    void operator()(std::string_view ip_address, http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        auto start_time = std::chrono::steady_clock::now();

        // 1. Логируем входной запрос
        logger::LogRequest(ip_address, req.target(), req.method_string());

        // 2. Обёртка над send для замера времени и логирования ответа
        auto logging_send = [start_time, send = std::forward<Send>(send)](auto&& response) {
            auto end_time = std::chrono::steady_clock::now();
            long long response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

            std::optional<std::string> content_type;
            if (response.find(http::field::content_type) != response.end()) {
                content_type = std::string(response[http::field::content_type]);
            }

            // Логируем отправляемый ответ
            logger::LogResponse(response_time, response.result_int(), content_type);

            // Отправляем ответ клиенту
            send(std::forward<decltype(response)>(response));
        };

        // 3. Вызываем основной обработчик
        decorated_(std::move(req), std::move(logging_send));
    }

private:
    SomeRequestHandler decorated_;
};
