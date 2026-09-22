#include "sdk.h"
#include <boost/asio.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <thread>

#include "json_loader.h"
#include "request_handler.h"
#include "logger.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::jthread> workers;
    workers.reserve(n - 1);
    while (--n) {
        workers.emplace_back(fn);
    }
    fn();
}

}  // namespace

int main(int argc, const char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: game_server <game-config-json> <folder_name>"sv << std::endl;
        return EXIT_FAILURE;
    }

    logger::InitLogger();

    int exit_code = EXIT_SUCCESS;
    std::optional<std::string> exception_msg;

    try {
        model::Game game = json_loader::LoadGame(argv[1]);
        std::string folder = argv[2];

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::strand<net::executor> strand(ioc.get_executor());

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        auto handler = std::make_shared<http_handler::RequestHandler>(game, folder, strand);

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        logger::LogServerStarted(port, address.to_string());

        http_server::ServeHttp(ioc, {address, port}, [handler](const std::string& ip, auto&& req, auto&& send) {
            (*handler)(ip, std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    } catch (const std::exception& ex) {
        exit_code = EXIT_FAILURE;
        exception_msg = ex.what();
    } catch (...) {
        exit_code = EXIT_FAILURE;
        exception_msg = "Unknown exception";
    }

    logger::LogServerExited(exit_code, exception_msg);
    return exit_code;
}
