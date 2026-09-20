#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>
#include <vector>

#include "json_loader.h"
#include "request_handler.h"

using namespace std::literals;

namespace {

template <typename Fn>
void RunWorkers(unsigned n, const Fn& fn) {
    n = std::max(1u, n);
    std::vector<std::thread> workers;
    workers.reserve(n - 1);
    for (unsigned i = 0; i < n - 1; ++i) {
        workers.emplace_back(fn);
    }
    fn();
    for (auto& w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }
}

}

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: game_server <game-config-json>"sv << std::endl;
        return EXIT_FAILURE;
    }
    try {
        model::Game game = json_loader::LoadGame(argv[1]);

        const unsigned num_threads = std::max(1u, std::thread::hardware_concurrency());
        boost::asio::io_context ioc(num_threads);

        boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const boost::system::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        const auto address = boost::asio::ip::make_address("0.0.0.0");
        constexpr boost::asio::ip::port_type port = 8080;

        http_handler::RequestHandler handler{game};

        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& sender) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(sender)>(sender));
        });

        std::cout << "Server has started..."sv << std::endl;

        RunWorkers(num_threads, [&ioc] {
            ioc.run();
        });
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
