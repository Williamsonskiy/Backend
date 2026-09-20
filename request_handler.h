#pragma once
#include "http_server.h"
#include "model.h"

#include <boost/json.hpp>
#include <string_view>
#include <string>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
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
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        send(HandleRequest(std::move(req)));
    }

private:
    using StringResponse = http::response<http::string_body>;

    template <typename Body, typename Allocator>
    StringResponse HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req) {
        const auto text_response = [&req](http::status status, std::string_view text) {
            StringResponse res{status, req.version()};
            res.set(http::field::content_type, "application/json");
            res.body() = text;
            res.content_length(text.size());
            res.keep_alive(req.keep_alive());
            return res;
        };

        std::string_view target = req.target();

        if (target.starts_with(NetEndpoint::api_prefix)) {
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

        boost::json::object obj;
        obj[std::string(JsonKey::code)] = std::string(JsonKey::bad_request);
        obj[std::string(JsonKey::message)] = std::string(JsonKey::bad_request_msg);
        return text_response(http::status::bad_request, boost::json::serialize(obj));
    }

    std::string MakeMapsListResponseBody() const;
    std::string MakeMapResponseBody(const model::Map& map) const;

    model::Game& game_;
};

}
