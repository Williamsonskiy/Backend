#pragma once
#include "sdk.h"

#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <string_view>
#include "model.h"

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

struct ContentType {
    ContentType() = delete;
    constexpr static std::string_view APPLICATION_JSON = "application/json"sv;
    constexpr static std::string_view TEXT_HTML = "text/html"sv;
};

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Send>
    void operator()(http::request<http::string_body>&& req, Send&& send) {
        send(HandleRequest(std::move(req)));
    }

private:
    http::response<http::string_body> HandleRequest(http::request<http::string_body>&& req);

    http::response<http::string_body> MakeJsonResponse(
        http::status status,
        std::string_view json_body,
        unsigned http_version,
        bool keep_alive) const;

    http::response<http::string_body> MakeBadRequestResponse(
        unsigned http_version,
        bool keep_alive) const;

    http::response<http::string_body> MakeNotFoundResponse(
        std::string_view code,
        std::string_view message,
        unsigned http_version,
        bool keep_alive) const;

    std::string SerializeMapList() const;
    std::string SerializeMap(const model::Map& map) const;

    model::Game& game_;
};

}  // namespace http_handler
