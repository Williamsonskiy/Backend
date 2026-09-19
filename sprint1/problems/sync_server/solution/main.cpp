#ifdef WIN32
#include <sdkddkver.h>
#endif

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <iostream>
#include <thread>
#include <optional>
#include <string>

namespace net = boost::asio;
using tcp = net::ip::tcp;
using namespace std::literals;
namespace beast = boost::beast;
namespace http = beast::http;

void HandleConnection(tcp::socket& socket) {
    beast::flat_buffer buffer;
    boost::system::error_code ec;

    while (true) {
        http::request<http::string_body> req;
        http::read(socket, buffer, req, ec);

        if (ec == http::error::end_of_stream) {
            break;
        }
        if (ec) {
            break;
        }

        if (req.method() == http::verb::get) {
            std::string target(req.target());
            target = target.substr(1);
            std::string body = "Hello, " + target;

            http::response<http::string_body> res(http::status::ok, req.version());
            res.set(http::field::content_type, "text/html");
            res.body() = body;
            res.content_length(body.size());
            res.keep_alive(req.keep_alive());

            http::write(socket, res, ec);
        } else if (req.method() == http::verb::head) {
            std::string target(req.target());
            target = target.substr(1);
            std::string body = "Hello, " + target;

            http::response<http::empty_body> res(http::status::ok, req.version());
            res.set(http::field::content_type, "text/html");
            res.content_length(body.size());
            res.keep_alive(req.keep_alive());

            http::write(socket, res, ec);
        } else {
            http::response<http::string_body> res(http::status::method_not_allowed, req.version());
            res.set(http::field::content_type, "text/html");
            res.set(http::field::allow, "GET, HEAD");
            res.body() = "Invalid method";
            res.content_length(14);
            res.keep_alive(req.keep_alive());

            http::write(socket, res, ec);
        }

        if (ec || !req.keep_alive()) {
            break;
        }
    }
}

int main() {
    net::io_context ioc;
    auto address = net::ip::make_address("0.0.0.0");
    unsigned short port = 8080;

    tcp::acceptor acceptor(ioc, {address, port});
    std::cout << "Server has started..."sv << std::endl;

    while (true) {
        tcp::socket socket(ioc);
        acceptor.accept(socket);
        HandleConnection(socket);
    }

    return 0;
}
