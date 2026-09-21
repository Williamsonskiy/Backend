#pragma once
#include <boost/json.hpp>
#include <boost/asio.hpp>
#include <filesystem>
#include <iostream>
#include "http_server.h"
#include "model.h"

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace sys = boost::system;
using namespace std::literals;
namespace fs = std::filesystem;
namespace net = boost::asio;

std::string urlDecode(const std::string& encodedString);
bool IsSubPath(fs::path path, fs::path base);
std::string getContentType(const fs::path& filePath);

void SetIdAndName(boost::json::object& json_map, const model::Map* map);
void SetRoads(boost::json::object& json_map, const model::Map* map);
void SetBuildings(boost::json::object& json_map, const model::Map* map);
void SetOffices(boost::json::object& json_map, const model::Map* map);

class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
public:
    explicit RequestHandler(model::Game& game, const std::string& folder, net::strand<net::executor>& strand)
        : game_{game}
        , folder_{fs::weakly_canonical(fs::path(folder))}
        , strand_(strand) {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        net::dispatch(strand_, [self = shared_from_this(), req = std::move(req), send = std::move(send)]() mutable {

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

        // 1. ИСПРАВЛЕНО замечание ревьюера:
        // Используем req.target() напрямую как string_view, БЕЗ .data()
        std::string_view target_view = req.target();
        
        // Отрезаем параметры запроса (всё, что после знака вопроса '?', если они есть)
        if (auto pos = target_view.find('?'); pos != std::string_view::npos) {
            target_view = target_view.substr(0, pos);
        }

        std::string path{target_view};

        if (path.starts_with(api)) {

            if (!path.starts_with(start)) {
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

            if (path.size() <= start.size() + 1) { // Первый тип запроса, не указана карта
                auto maps = self->game_.GetMaps();
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
            } else { // Второй тип запроса
                auto id_start = path.find_last_of('/') + 1;
                std::string map_id = path.substr(id_start);
                const model::Map* map = self->game_.FindMap(model::Map::Id(map_id));
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

            if (path == "/"s) {
                path = "/index.html"s;
            }

            std::string decoded_path = urlDecode(path);
            
            // 2. ИСПРАВЛЕНИЕ путей: превращаем путь в относительный
            // гарантированно отрезая начальный слэш для безопасного объединения путей
            std::string rel_path_str;
            if (!decoded_path.empty() && decoded_path[0] == '/') {
                rel_path_str = decoded_path.substr(1);
            } else {
                rel_path_str = decoded_path;
            }

            fs::path uri_path(rel_path_str);
            uri_path = uri_path.lexically_normal();

            fs::path abs_path = fs::weakly_canonical(self->folder_ / uri_path);

            if (IsSubPath(abs_path, self->folder_)) {
                // Добавлена проверка is_regular_file
                if (fs::exists(abs_path) && fs::is_regular_file(abs_path)) {
                    http::response<http::file_body> res;
                    res.version(11);
                    std::string contentType = getContentType(abs_path);
                    res.set(http::field::content_type, contentType);
                    res.result(http::status::ok);

                    beast::error_code ec;
                    http::file_body::value_type body;

                    if (body.open(abs_path.c_str(), beast::file_mode::read, ec), ec) {
                        std::cerr << "Failed to open file "sv << abs_path.c_str() << std::endl;
                        return;
                    }

                    res.body() = std::move(body);
                    res.prepare_payload();
                    send(std::move(res));
                    return;
                } else {
                    http::response<http::string_body> res;
                    res.version(11);
                    res.result(http::status::not_found);
                    res.insert(http::field::content_type, "text/plain"sv);
                    res.body() = "404 Not Found File";
                    res.prepare_payload();
                    send(std::move(res));
                    return;
                }
            } else {
                http::response<http::string_body> res;
                res.version(11);
                res.result(http::status::bad_request);
                res.insert(http::field::content_type, "text/plain"sv);
                res.body() = "Bad request";
                res.prepare_payload();
                send(std::move(res));
                return;
            }
        }

        });
    }

private:
    model::Game& game_;
    fs::path folder_;
    net::strand<net::executor>& strand_;
};

}  // namespace http_handler
