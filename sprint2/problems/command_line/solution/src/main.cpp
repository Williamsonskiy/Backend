#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <thread>
#include <optional>
#include <vector>
#include "json_loader.h"
#include "request_handler.h"
#include "app.h"
#include "logger.h"
#include "http_server.h"
#include "ticker.h"

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

struct Args {
    std::string config_file;
    std::string www_root;
    std::optional<int> tick_period;
    bool randomize_spawn_points = false;
};

std::optional<Args> ParseCommandLine(int argc, char* argv[]) {
    namespace po = boost::program_options;
    
    po::options_description visible_desc("Allowed options");
    visible_desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<std::vector<int>>()->value_name("milliseconds"), "set tick period")
        ("config-file,c", po::value<std::vector<std::string>>()->value_name("file"), "set config file path")
        ("www-root,w", po::value<std::vector<std::string>>()->value_name("dir"), "set static files root")
        ("randomize-spawn-points", "spawn dogs at random positions");

    // Скрытая опция для перехвата любых лишних позиционных аргументов из старого ENTRYPOINT
    po::options_description hidden_desc("Hidden options");
    hidden_desc.add_options()
        ("positional", po::value<std::vector<std::string>>());

    po::options_description all_desc("All options");
    all_desc.add(visible_desc).add(hidden_desc);

    po::positional_options_description positional_desc;
    positional_desc.add("positional", -1); // Все аргументы без флагов летят сюда

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv)
                  .options(all_desc)
                  .positional(positional_desc)
                  .allow_unregistered()
                  .run(), vm);
    po::notify(vm);

    if (vm.contains("help")) {
        std::cout << visible_desc << "\n";
        return std::nullopt;
    }

    Args args;

    // Пытаемся достать пути либо из флагов -c и -w, либо из позиционных аргументов
    if (vm.contains("config-file")) {
        args.config_file = vm["config-file"].as<std::vector<std::string>>().back();
    } else if (vm.contains("positional")) {
        const auto& pos_args = vm["positional"].as<std::vector<std::string>>();
        if (pos_args.size() > 0) {
            args.config_file = pos_args[0];
        }
    }

    if (vm.contains("www-root")) {
        args.www_root = vm["www-root"].as<std::vector<std::string>>().back();
    } else if (vm.contains("positional")) {
        const auto& pos_args = vm["positional"].as<std::vector<std::string>>();
        if (pos_args.size() > 1) {
            args.www_root = pos_args[1];
        }
    }

    if (args.config_file.empty()) {
        throw std::runtime_error("Config file path is required");
    }
    if (args.www_root.empty()) {
        throw std::runtime_error("Static files root is required");
    }

    if (vm.contains("tick-period")) {
        args.tick_period = vm["tick-period"].as<std::vector<int>>().back();
    }

    if (vm.contains("randomize-spawn-points")) {
        args.randomize_spawn_points = true;
    }

    return args;
}

} // namespace

int main(int argc, char* argv[]) {
    logger::InitBoostLogFilter();

    std::optional<Args> args;
    try {
        args = ParseCommandLine(argc, argv);
        if (!args) {
            return EXIT_SUCCESS;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing command line: " << e.what() << std::endl;
        logger::LogServerExited(EXIT_FAILURE, e.what());
        return EXIT_FAILURE;
    }

    try {
        model::Game game = json_loader::LoadGame(args->config_file);
        game.SetRandomizedSpawn(args->randomize_spawn_points);

        bool auto_tick = args->tick_period.has_value();
        app::App app(game, auto_tick);

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        auto api_strand = net::make_strand(ioc);

        std::shared_ptr<ticker::Ticker> ticker;
        if (auto_tick) {
            ticker = std::make_shared<ticker::Ticker>(
                api_strand,
                std::chrono::milliseconds(*args->tick_period),
                [&app](std::chrono::milliseconds delta) {
                    app.Tick(delta);
                }
            );
            ticker->Start();
        }

        http_handler::RequestHandler handler{app, args->www_root, api_strand};
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
