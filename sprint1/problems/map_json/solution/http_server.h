#pragma once
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <memory>
#include <string_view>
#include <utility>

namespace http_server {

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace beast = boost::beast;
namespace http = beast::http;

using namespace std::literals;

void ReportError(beast::error_code ec, std::string_view what);

template <typename RequestHandler>
class Session : public std::enable_shared_from_this<Session<RequestHandler>> {
public:
    // Принимаем RequestHandler по значению для корректной передачи из std::make_shared
    Session(tcp::socket&& socket, RequestHandler request_handler)
        : stream_(std::move(socket))
        , request_handler_(std::move(request_handler)) {
    }

    void Run() {
        net::dispatch(
            stream_.get_executor(),
            beast::bind_front_handler(&Session::DoRead, this->shared_from_this()));
    }

private:
    void DoRead() {
        req_ = {};
        stream_.expires_after(std::chrono::seconds(30));
        http::async_read(
            stream_, buffer_, req_,
            beast::bind_front_handler(&Session::OnRead, this->shared_from_this()));
    }

    void OnRead(beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec == http::error::end_of_stream) {
            return Close();
        }
        if (ec) {
            return ReportError(ec, "read"sv);
        }
        HandleRequest();
    }

    void HandleRequest() {
        request_handler_(
            stream_.socket().remote_endpoint(),
            std::move(req_),
            [self = this->shared_from_this()](auto&& response) {
                self->Write(std::move(response));
            });
    }

    template <typename Response>
    void Write(Response&& response) {
        auto safe_response = std::make_shared<std::decay_t<Response>>(std::forward<Response>(response));
        http::async_write(
            stream_, *safe_response,
            [self = this->shared_from_this(), safe_response](beast::error_code ec, std::size_t bytes_written) {
                self->OnWrite(safe_response->need_eof(), ec, bytes_written);
            });
    }

    void OnWrite(bool close, beast::error_code ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);
        if (ec) {
            return ReportError(ec, "write"sv);
        }
        if (close) {
            return Close();
        }
        DoRead();
    }

    void Close() {
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    }

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    RequestHandler request_handler_;
};

template <typename RequestHandler>
class Listener : public std::enable_shared_from_this<Listener<RequestHandler>> {
public:
    template <typename Handler>
    Listener(net::io_context& ioc, const tcp::endpoint& endpoint, Handler&& handler)
        : ioc_(ioc)
        , acceptor_(net::make_strand(ioc))
        , request_handler_(std::forward<Handler>(handler)) {
        beast::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            ReportError(ec, "open"sv);
            return;
        }

        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) {
            ReportError(ec, "set_option"sv);
            return;
        }

        acceptor_.bind(endpoint, ec);
        if (ec) {
            ReportError(ec, "bind"sv);
            return;
        }

        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) {
            ReportError(ec, "listen"sv);
            return;
        }
    }

    void Run() {
        DoAccept();
    }

private:
    void DoAccept() {
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(&Listener::OnAccept, this->shared_from_this()));
    }

    void OnAccept(beast::error_code ec, tcp::socket socket) {
        if (ec) {
            return ReportError(ec, "accept"sv);
        }
        AsyncRunSession(std::move(socket));
        DoAccept();
    }

    void AsyncRunSession(tcp::socket&& socket) {
        std::make_shared<Session<RequestHandler>>(std::move(socket), request_handler_)->Run();
    }

    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    RequestHandler request_handler_;
};

template <typename RequestHandler>
void ServeHttp(net::io_context& ioc, const tcp::endpoint& endpoint, RequestHandler&& handler) {
    using MyListener = Listener<std::decay_t<RequestHandler>>;
    std::make_shared<MyListener>(ioc, endpoint, std::forward<RequestHandler>(handler))->Run();
}

}  // namespace http_server
