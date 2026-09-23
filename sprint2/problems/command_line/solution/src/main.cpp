#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/json.hpp>
#include <boost/program_options.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <thread>
#include <vector>

#include "json_loader.h"
#include "logger.h"
#include "request_handler.h"
#include "ticker.h"

using namespace std::literals;

namespace net = boost::asio;
namespace sys = boost::system;
namespace fs = std::filesystem;
namespace json = boost::json;

namespace {

struct Args {
    fs::path config_file;
    fs::path www_root;
    std::optional<int> tick_period;
    bool randomize_spawn_points = false;
};

[[nodiscard]] std::optional<Args> ParseCommandLine(int argc, const char* const argv[]) {
    namespace po = boost::program_options;

    Args args;

    po::options_description desc{"Allowed options"s};

    desc.add_options()
        ("help,h", "produce help message")
        ("tick-period,t", po::value<int>()->value_name("milliseconds"), "set tick period")
        ("config-file,c", po::value<std::string>()->value_name("file")->required(), "set config file path")
        ("www-root,w", po::value<std::string>()->value_name("dir")->required(), "set static files root")
        ("randomize-spawn-points", "spawn dogs at random positions");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.contains("help"s)) {
        std::cout << desc << std::endl;
        return std::nullopt;
    }

    po::notify(vm);

    args.config_file = vm["config-file"s].as<std::string>();
    args.www_root = vm["www-root"s].as<std::string>();

    if (vm.contains("tick-period"s)) {
        args.tick_period = vm["tick-period"s].as<int>();
    }

    args.randomize_spawn_points = vm.contains("randomize-spawn-points"s);

    return args;
}

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
    try {
        InitLogging();

        const auto args = ParseCommandLine(argc, argv);

        if (!args) {
            return EXIT_SUCCESS;
        }

        model::Game game = json_loader::LoadGame(args->config_file);
        game.SetRandomizeSpawnPoints(args->randomize_spawn_points);

        const unsigned num_threads = std::max(1u, std::thread::hardware_concurrency());
        net::io_context ioc(static_cast<int>(num_threads));

        auto api_strand = net::make_strand(ioc);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        const bool manual_tick_enabled = !args->tick_period.has_value();

        http_handler::RequestHandler handler{
            game,
            args->www_root,
            manual_tick_enabled
        };

        std::shared_ptr<Ticker> ticker;

        if (args->tick_period) {
            ticker = std::make_shared<Ticker>(
                api_strand,
                std::chrono::milliseconds{*args->tick_period},
                [&game](std::chrono::milliseconds delta) {
                    const double delta_seconds = static_cast<double>(delta.count()) / 1000.0;
                    game.Update(delta_seconds);
                }
            );

            ticker->Start();
        }

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        LogInfo("server started", json::object{
            {"port", port},
            {"address", address.to_string()}
        });

        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler(
                std::forward<decltype(req)>(req),
                std::forward<decltype(send)>(send)
            );
        });

        RunWorkers(num_threads, [&ioc] {
            ioc.run();
        });

        LogInfo("server exited", json::object{
            {"code", 0}
        });

        return EXIT_SUCCESS;

    } catch (const std::exception& ex) {
        InitLogging();

        LogInfo("server exited", json::object{
            {"code", EXIT_FAILURE},
            {"exception", ex.what()}
        });

        return EXIT_FAILURE;
    }
}
