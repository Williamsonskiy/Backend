#include "sdk.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <thread>

#include "json_loader.h"
#include "request_handler.h"

namespace net = boost::asio;
namespace sys = boost::system;
namespace http = boost::beast::http;

namespace {

// Функция запускает `count` рабочих потоков io_context
template <typename Fn>
void RunWorkers(unsigned count, const Fn& fn) { offset:
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

}  // namespace

int main(int argc, const char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: game_server <path-to-json-config> <path-to-static-files>" << std::endl;
        return EXIT_FAILURE;
    }

    try {
        // 1. Загружаем карту из JSON-файла
        model::Game game = json_loader::LoadGame(argv[1]);

        // Путь к статическим файлам из 2-го аргумента
        std::filesystem::path static_path{argv[2]};

        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 3. Добавляем обработчик сигналов для изящного завершения работы
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, int signal_number) {
            if (!ec) {
                ioc.stop();
            }
        });

        // 4. Создаем обработчик HTTP-запросов
        http_handler::RequestHandler handler{game, static_path};

        // 5. Запускаем HTTP-сервер
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        // ВАЖНО для тестов: Сообщение о старте сервера и очистка буфера std::endl
        std::cout << "Server started at port " << port << std::endl;

        // 6. Запускаем обработку асинхронных операций
        RunWorkers(std::max(1u, num_threads), [&ioc] {
            ioc.run();
        });
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
