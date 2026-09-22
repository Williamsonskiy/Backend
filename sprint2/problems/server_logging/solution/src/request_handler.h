#pragma once

#include <boost/json.hpp>
#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include "logger.h"

namespace http = boost::beast::http;

template <typename SomeRequestHandler>
class LoggingRequestHandler {
public:
    explicit LoggingRequestHandler(SomeRequestHandler&& decorated)
        : decorated_(std::move(decorated)) {}

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, std::string_view ip_address, Send&& send) {
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
