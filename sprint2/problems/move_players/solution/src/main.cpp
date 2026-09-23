#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <iostream>
#include <thread>
#include "json_loader.h"
#include "request_handler.h"
#include "app.h"
#include "logger.h"
#include "http_server.h"

namespace net = boost::asio;
namespace sys = boost::system;

namespace {

template <typename Fn>
void RunWorkers(unsigned int count, const Fn& fn) {
    count = std::max(1u, count);
    std::vector<std::thread> v;
    v.reserve(count - 1);
    for (auto i = count - 1; i > 0; --i) {
        v.emplace_back(fn);
    }
    fn();
    for (auto& t : v) {
        t.join();
    }
}

} // namespace

int main(int argc, char* argv[]) {
    logger::InitBoostLogFilter();

    if (argc != 3) {
        std::cerr << "Usage: game_server <game-config-json> <static-files-path>" << std::endl;
        logger::LogServerExited(EXIT_FAILURE, "Invalid command line arguments");
        return EXIT_FAILURE;
    }

    try {
        model::Game game = json_loader::LoadGame(argv[1]);
        app::App app(game);

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        // Создаем Strand для потокобезопасной обработки API вызовов
        auto api_strand = net::make_strand(ioc);
        http_handler::RequestHandler handler{app, argv[2], api_strand};
        LoggingRequestHandler logging_handler{std::move(handler)};

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        http_server::ServeHttp(ioc, {address, port}, logging_handler);
        logger::LogServerStarted(address.to_string(), port);

        RunWorkers(num_threads, [&ioc] {
            ioc.run();
        });

        logger::LogServerExited(0);
    } catch (const std::exception& ex) {
        logger::LogServerExited(EXIT_FAILURE, ex.what());
        return EXIT_FAILURE;
    }

    return 0;
}
