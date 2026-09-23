#pragma once

#include "api_handler.h"
#include "logger.h"
#include <boost/asio.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/http.hpp>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <filesystem>

namespace http = boost::beast::http;

namespace http_handler {

namespace fs = std::filesystem;

std::string urlDecode(const std::string& encodedString);
bool IsSubPath(fs::path path, fs::path base);
std::string getContentType(const fs::path& filePath);

class RequestHandler {
public:
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;

    explicit RequestHandler(app::App& app, fs::path static_path, Strand api_strand)
        : api_handler_(app)
        , static_path_{fs::weakly_canonical(std::move(static_path))}
        , api_strand_(api_strand) {
    }

    RequestHandler(const RequestHandler&) = default;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        std::string target(req.target());
        target = urlDecode(target);

        // --- Обработка API с гарантией последовательного выполнения через strand ---
        if (target.starts_with("/api/")) {
            auto handle = [this, req = std::move(req), send_copy = std::forward<Send>(send)]() mutable {
                api_handler_.HandleRequest(std::move(req), std::move(send_copy));
            };
            return boost::asio::dispatch(api_strand_, std::move(handle));
        }

        // --- Обработка статических файлов ---
        auto text_response = [&req](http::status status, std::string_view text, std::string_view content_type) {
            http::response<http::string_body> response(status, req.version());
            response.set(http::field::content_type, content_type);
            response.body() = std::string(text);
            response.content_length(text.size());
            response.keep_alive(req.keep_alive());
            return response;
        };

        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            return send(text_response(http::status::method_not_allowed, "Invalid method", "text/plain"));
        }

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
    ApiHandler api_handler_;
    fs::path static_path_;
    Strand api_strand_;
};

} // namespace http_handler

template <typename SomeRequestHandler>
class LoggingRequestHandler {
public:
    LoggingRequestHandler(const LoggingRequestHandler&) = default;
    LoggingRequestHandler(LoggingRequestHandler&&) = default;

    explicit LoggingRequestHandler(SomeRequestHandler&& decorated)
        : decorated_(std::move(decorated)) {}

    template <typename Body, typename Allocator, typename Send>
    void operator()(std::string_view ip_address, http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        auto start_time = std::chrono::steady_clock::now();
        logger::LogRequest(ip_address, req.target(), req.method_string());

        auto logging_send = [start_time, send = std::forward<Send>(send)](auto&& response) {
            auto end_time = std::chrono::steady_clock::now();
            long long response_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

            std::optional<std::string> content_type;
            if (response.find(http::field::content_type) != response.end()) {
                content_type = std::string(response[http::field::content_type]);
            }

            logger::LogResponse(response_time, response.result_int(), content_type);
            send(std::forward<decltype(response)>(response));
        };

        decorated_(std::move(req), std::move(logging_send));
    }

private:
    SomeRequestHandler decorated_;
};
