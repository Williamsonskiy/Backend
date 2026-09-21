#pragma once
#include "http_server.h"
#include "model.h"

#include <boost/json.hpp>
#include <filesystem>
#include <string_view>
#include <string>
#include <iostream>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;
namespace fs = std::filesystem;
using namespace std::literals;

std::string UrlDecode(std::string_view encodedString);
bool IsSubPath(fs::path path, fs::path base);
std::string GetContentType(const fs::path& filePath);

void SetIdAndName(boost::json::object& json_map, const model::Map* map);
void SetRoads(boost::json::object& json_map, const model::Map* map);
void SetBuildings(boost::json::object& json_map, const model::Map* map);
void SetOffices(boost::json::object& json_map, const model::Map* map);

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, const fs::path& static_root)
        : game_{game}
        , static_root_{fs::weakly_canonical(static_root)} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        
        // Проверка метода (разрешены только GET и HEAD)
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            boost::json::object error_json = {
                {"code", "badRequest"},
                {"message", "Method not allowed"}
            };

            http::response<http::string_body> res{http::status::method_not_allowed, req.version()};
            res.set(http::field::content_type, "application/json");
            res.body() = boost::json::serialize(error_json);
            res.prepare_payload();

            send(std::move(res));
            return;
        }

        const std::string start = "/api/v1/maps";
        const std::string api = "/api/";

        // Исправляем ошибку: берем target напрямую как string_view, БЕЗ .data()
        std::string_view target = req.target();
        
        // Удаляем параметры запроса из URL (всё, что после '?')
        if (auto pos = target.find('?'); pos != std::string_view::npos) {
            target = target.substr(0, pos);
        }

        if (target.starts_with(api)) {
            if (!target.starts_with(start)) {
                boost::json::object error_json = {
                    {"code", "badRequest"},
                    {"message", "Bad request"}
                };
                http::response<http::string_body> res{http::status::bad_request, req.version()};
                res.set(http::field::content_type, "application/json");
                res.body() = boost::json::serialize(error_json);
                res.prepare_payload();
                send(std::move(res));
                return;
            }

            if (target.size() <= start.size() + 1) { // Первый тип запроса (список карт)
                auto maps = game_.GetMaps();
                boost::json::array maps_array;
                for (const auto& map : maps) {
                    boost::json::object map_json;
                    map_json["id"] = *map.GetId();
                    map_json["name"] = map.GetName();
                    maps_array.push_back(map_json);
                }

                http::response<http::string_body> res{http::status::ok, req.version()};
                res.set(http::field::content_type, "application/json");
                res.body() = boost::json::serialize(maps_array);
                res.prepare_payload();
                send(std::move(res));
            } else { // Второй тип запроса (конкретная карта)
                auto id_start = target.find_last_of('/') + 1;
                std::string map_id{target.substr(id_start)};
                
                const model::Map* map = game_.FindMap(model::Map::Id(map_id));
                if (!map) {
                    boost::json::object error_json = {
                        {"code", "mapNotFound"},
                        {"message", "Map not found"}
                    };
                    http::response<http::string_body> res{http::status::not_found, req.version()};
                    res.set(http::field::content_type, "application/json");
                    res.body() = boost::json::serialize(error_json);
                    res.prepare_payload();
                    send(std::move(res));
                    return;
                }

                boost::json::object json_map;
                SetIdAndName(json_map, map);
                SetRoads(json_map, map);
                SetBuildings(json_map, map);
                SetOffices(json_map, map);

                http::response<http::string_body> res{http::status::ok, req.version()};
                res.set(http::field::content_type, "application/json");
                res.body() = boost::json::serialize(json_map);
                res.prepare_payload();
                send(std::move(res));
                return;
            }
        } else {
            // Статика
            std::string path_str{target};
            if (path_str == "/") {
                path_str = "/index.html";
            }

            fs::path de_path{UrlDecode(path_str)};
            fs::path uri_path = de_path.lexically_normal();

            if (uri_path.has_root_path()) {
                uri_path = uri_path.relative_path(); // Отрезает начальный слэш для конкатенации
            }

            fs::path abs_path = fs::weakly_canonical(static_root_ / uri_path);

            if (IsSubPath(abs_path, static_root_)) {
                if (fs::exists(abs_path) && fs::is_regular_file(abs_path)) {
                    http::response<http::file_body> res;
                    res.version(req.version());
                    std::string contentType = GetContentType(abs_path);
                    res.set(http::field::content_type, contentType);
                    res.result(http::status::ok);

                    beast::error_code ec;
                    http::file_body::value_type body;

                    if (body.open(abs_path.c_str(), beast::file_mode::read, ec); ec) {
                        std::cerr << "Failed to open file "sv << abs_path.c_str() << std::endl;
                        http::response<http::string_body> err_res{http::status::internal_server_error, req.version()};
                        send(std::move(err_res));
                        return;
                    }

                    res.body() = std::move(body);
                    res.prepare_payload();
                    send(std::move(res));
                    return;
                } else {
                    http::response<http::string_body> res;
                    res.version(req.version());
                    res.result(http::status::not_found);
                    res.set(http::field::content_type, "text/plain"sv);
                    res.body() = "404 Not Found File";
                    res.prepare_payload();
                    send(std::move(res));
                    return;
                }
            } else {
                http::response<http::string_body> res;
                res.version(req.version());
                res.result(http::status::bad_request);
                res.set(http::field::content_type, "text/plain"sv);
                res.body() = "Bad request";
                res.prepare_payload();
                send(std::move(res));
                return;
            }
        }
    }

private:
    model::Game& game_;
    fs::path static_root_;
};

}  // namespace http_handler
