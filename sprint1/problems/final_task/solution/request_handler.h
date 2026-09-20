#pragma once
#include "http_server.h"
#include "model.h"

#include <boost/json.hpp>
#include <string_view>

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
using namespace std::literals;

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

        if (target.starts_with("/api/")) {
            if (target == "/api/v1/maps"sv) {
                if (req.method() == http::verb::get || req.method() == http::verb::head) {
                    return text_response(http::status::ok, MakeMapsListResponseBody());
                }
            } else if (target.starts_with("/api/v1/maps/"sv)) {
                if (target.size() > "/api/v1/maps/"sv.size()) {
                    if (req.method() == http::verb::get || req.method() == http::verb::head) {
                        std::string_view map_id = target.substr("/api/v1/maps/"sv.size());
                        auto map = game_.FindMap(model::Map::Id{std::string(map_id)});
                        if (map) {
                            return text_response(http::status::ok, MakeMapResponseBody(*map));
                        } else {
                            boost::json::object obj;
                            obj["code"] = "mapNotFound";
                            obj["message"] = "Map not found";
                            return text_response(http::status::not_found, boost::json::serialize(obj));
                        }
                    }
                }
            }

            boost::json::object obj;
            obj["code"] = "badRequest";
            obj["message"] = "Bad request";
            return text_response(http::status::bad_request, boost::json::serialize(obj));
        }

        boost::json::object obj;
        obj["code"] = "badRequest";
        obj["message"] = "Bad request";
        return text_response(http::status::bad_request, boost::json::serialize(obj));
    }

    std::string MakeMapsListResponseBody() const;
    std::string MakeMapResponseBody(const model::Map& map) const;

    model::Game& game_;
};

}
