#pragma once
#include "http_server.h"
#include "model.h"

#include <boost/json.hpp>
#include <filesystem>
#include <string_view>
#include <string>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
namespace fs = std::filesystem;
using namespace std::literals;

namespace NetEndpoint {
constexpr std::string_view api_v1_maps = "/api/v1/maps";
constexpr std::string_view api_v1_maps_prefix = "/api/v1/maps/";
constexpr std::string_view api_prefix = "/api/";
}

namespace JsonKey {
constexpr std::string_view code = "code";
constexpr std::string_view message = "message";
constexpr std::string_view map_not_found = "mapNotFound";
constexpr std::string_view map_not_found_msg = "Map not found";
constexpr std::string_view bad_request = "badRequest";
constexpr std::string_view bad_request_msg = "Bad request";
}

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, fs::path static_root)
        : game_{game}
        , static_root_{fs::weakly_canonical(static_root)} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string_view target = req.target();

        if (target.starts_with(NetEndpoint::api_prefix)) {
            send(HandleApiRequest(std::move(req)));
        } else {
            HandleStaticFileRequest(std::move(req), std::forward<Send>(send));
        }
    }

private:
    using StringResponse = http::response<http::string_body>;

    template <typename Body, typename Allocator>
    StringResponse HandleApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req) {
        const auto text_response = [&req](http::status status, std::string_view text) {
            StringResponse res{status, req.version()};
            res.set(http::field::content_type, "application/json");
            res.set(http::field::cache_control, "no-cache");
            res.keep_alive(req.keep_alive());
            if (req.method() != http::verb::head) {
                res.body() = std::string(text);
            }
            res.content_length(text.size());
            return res;
        };

        std::string_view target = req.target();

        if (target == NetEndpoint::api_v1_maps) {
            if (req.method() == http::verb::get || req.method() == http::verb::head) {
                return text_response(http::status::ok, MakeMapsListResponseBody());
            }
        } else if (target.starts_with(NetEndpoint::api_v1_maps_prefix)) {
            if (target.size() > NetEndpoint::api_v1_maps_prefix.size()) {
                if (req.method() == http::verb::get || req.method() == http::verb::head) {
                    std::string_view map_id = target.substr(NetEndpoint::api_v1_maps_prefix.size());
                    auto map = game_.FindMap(model::Map::Id{std::string(map_id)});
                    if (map) {
                        return text_response(http::status::ok, MakeMapResponseBody(*map));
                    } else {
                        boost::json::object obj;
                        obj[std::string(JsonKey::code)] = std::string(JsonKey::map_not_found);
                        obj[std::string(JsonKey::message)] = std::string(JsonKey::map_not_found_msg);
                        return text_response(http::status::not_found, boost::json::serialize(obj));
                    }
                }
            }
        }

        boost::json::object obj;
        obj[std::string(JsonKey::code)] = std::string(JsonKey::bad_request);
        obj[std::string(JsonKey::message)] = std::string(JsonKey::bad_request_msg);
        return text_response(http::status::bad_request, boost::json::serialize(obj));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleStaticFileRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        const auto make_plain_response = [&req](http::status status, std::string_view body) {
            StringResponse res{status, req.version()};
            res.set(http::field::content_type, "text/plain");
            res.keep_alive(req.keep_alive());
            if (req.method() != http::verb::head) {
                res.body() = std::string(body);
            }
            res.content_length(body.size());
            return res;
        };

        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            send(make_plain_response(http::status::method_not_allowed, "Invalid method"sv));
            return;
        }

        std::string decoded_url = UrlDecode(req.target());
        if (decoded_url.empty() || decoded_url[0] != '/') {
            send(make_plain_response(http::status::bad_request, "Bad request"sv));
            return;
        }

        fs::path rel_path = fs::path(decoded_url).relative_path();
        fs::path req_path = static_root_ / rel_path;

        if (fs::is_directory(req_path)) {
            req_path /= "index.html";
        }

        fs::path canonical_req_path = fs::weakly_canonical(req_path);

        if (!IsSubPath(canonical_req_path, static_root_)) {
            send(make_plain_response(http::status::bad_request, "Bad request"sv));
            return;
        }

        beast::error_code ec;
        http::file_body::value_type body;
        body.open(canonical_req_path.string().c_str(), beast::file_mode::read, ec);

        if (ec == beast::errc::no_such_file_or_directory || ec == beast::errc::not_a_directory) {
            send(make_plain_response(http::status::not_found, "File not found"sv));
            return;
        }

        if (ec) {
            send(make_plain_response(http::status::internal_server_error, "Server error"sv));
            return;
        }

        auto const size = body.size();
        std::string mime = GetMimeType(canonical_req_path.extension().string());

        if (req.method() == http::verb::head) {
            http::response<http::empty_body> res{http::status::ok, req.version()};
            res.set(http::field::content_type, mime);
            res.content_length(size);
            res.keep_alive(req.keep_alive());
            send(std::move(res));
            return;
        }

        http::response<http::file_body> res{
            std::piecewise_construct,
            std::make_tuple(std::move(body)),
            std::make_tuple(http::status::ok, req.version())};
        res.set(http::field::content_type, mime);
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        send(std::move(res));
    }

    static std::string UrlDecode(std::string_view src);
    static std::string GetMimeType(std::string_view ext);
    static bool IsSubPath(fs::path path, fs::path base);

    std::string MakeMapsListResponseBody() const;
    std::string MakeMapResponseBody(const model::Map& map) const;

    model::Game& game_;
    fs::path static_root_;
};

}  // namespace http_handler
