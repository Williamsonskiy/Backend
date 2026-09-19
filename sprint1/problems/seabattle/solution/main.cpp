#ifdef WIN32
#include <sdkddkver.h>
#endif

#include "seabattle.h"

#include <atomic>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <string_view>

namespace net = boost::asio;
using net::ip::tcp;
using namespace std::literals;

void PrintFieldPair(const SeabattleField& left, const SeabattleField& right) {
    auto left_pad = "  "s;
    auto delimeter = "    "s;
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
    for (size_t i = 0; i < SeabattleField::field_size; ++i) {
        std::cout << left_pad;
        left.PrintLine(std::cout, i);
        std::cout << delimeter;
        right.PrintLine(std::cout, i);
        std::cout << std::endl;
    }
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
}

template <size_t sz>
static std::optional<std::string> ReadExact(tcp::socket& socket) {
    boost::array<char, sz> buf;
    boost::system::error_code ec;

    net::read(socket, net::buffer(buf), net::transfer_exactly(sz), ec);

    if (ec) {
        return std::nullopt;
    }

    return {{buf.data(), sz}};
}

static bool WriteExact(tcp::socket& socket, std::string_view data) {
    boost::system::error_code ec;

    net::write(socket, net::buffer(data), net::transfer_exactly(data.size()), ec);

    return !ec;
}

class SeabattleAgent {
public:
    SeabattleAgent(const SeabattleField& field)
        : my_field_(field) {
    }

    void StartGame(tcp::socket& socket, bool my_initiative) {
        while (!IsGameEnded()) {
            PrintFields();
            if (my_initiative) {
                std::cout << "Your turn: ";
                std::string move_str;
                std::cin >> move_str;
                auto parsed_move = ParseMove(move_str);
                if (!parsed_move) {
                    continue;
                }
                
                SendMove(socket, MoveToString(*parsed_move));
                
                auto result = ReadResult(socket);
                if (!result) {
                    return;
                }
                
                int x = parsed_move->first;
                int y = parsed_move->second;
                
                if (*result == SeabattleField::ShotResult::MISS) {
                    other_field_.MarkMiss(x, y);
                    my_initiative = false;
                } else if (*result == SeabattleField::ShotResult::HIT) {
                    other_field_.MarkHit(x, y);
                } else if (*result == SeabattleField::ShotResult::KILL) {
                    other_field_.MarkKill(x, y);
                }
            } else {
                std::cout << "Waiting for turn..." << std::endl;
                
                auto move = ReadMove(socket);
                if (!move) {
                    return;
                }
                
                int x = move->first;
                int y = move->second;
                
                auto shot_res = my_field_.Shoot(x, y);
                SendResult(socket, shot_res);
                
                if (shot_res == SeabattleField::ShotResult::MISS) {
                    my_initiative = true;
                }
            }
        }
        PrintFields();
    }

private:
    static std::optional<std::pair<int, int>> ParseMove(const std::string_view& sv) {
        if (sv.size() != 2) return std::nullopt;

        int p1 = sv[0] - 'A', p2 = sv[1] - '1';

        if (p1 < 0 || p1 > 8) return std::nullopt;
        if (p2 < 0 || p2 > 8) return std::nullopt;

        return {{p1, p2}};
    }

    static std::string MoveToString(std::pair<int, int> move) {
        char buff[] = {static_cast<char>(move.first) + 'A', static_cast<char>(move.second) + '1'};
        return {buff, 2};
    }

    void PrintFields() const {
        PrintFieldPair(my_field_, other_field_);
    }

    bool IsGameEnded() const {
        return my_field_.IsLoser() || other_field_.IsLoser();
    }

    void SendMove(tcp::socket& socket, const std::string& move_str) {
        WriteExact(socket, move_str);
    }

    std::optional<SeabattleField::ShotResult> ReadResult(tcp::socket& socket) {
        auto res = ReadExact<1>(socket);
        if (!res) return std::nullopt;
        return static_cast<SeabattleField::ShotResult>(res->at(0));
    }

    void SendResult(tcp::socket& socket, SeabattleField::ShotResult result) {
        char res_byte = static_cast<char>(result);
        WriteExact(socket, std::string_view(&res_byte, 1));
    }

    std::optional<std::pair<int, int>> ReadMove(tcp::socket& socket) {
        auto move_opt = ReadExact<2>(socket);
        if (!move_opt) return std::nullopt;
        return ParseMove(*move_opt);
    }

private:
    SeabattleField my_field_;
    SeabattleField other_field_;
};

void StartServer(const SeabattleField& field, unsigned short port) {
    SeabattleAgent agent(field);
    net::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
    boost::system::error_code ec;
    tcp::socket socket(io_context);
    
    acceptor.accept(socket, ec);
    if (!ec) {
        agent.StartGame(socket, false);
    }
}

void StartClient(const SeabattleField& field, const std::string& ip_str, unsigned short port) {
    SeabattleAgent agent(field);
    net::io_context io_context;
    tcp::socket socket(io_context);
    boost::system::error_code ec;
    
    socket.connect(tcp::endpoint(net::ip::make_address(ip_str, ec), port), ec);
    if (!ec) {
        agent.StartGame(socket, true);
    }
}

int main(int argc, const char** argv) {
    if (argc != 3 && argc != 4) {
        std::cout << "Usage: program <seed> [<ip>] <port>" << std::endl;
        return 1;
    }

    std::mt19937 engine(std::stoi(argv[1]));
    SeabattleField fieldL = SeabattleField::GetRandomField(engine);

    if (argc == 3) {
        StartServer(fieldL, std::stoi(argv[2]));
    } else if (argc == 4) {
        StartClient(fieldL, argv[2], std::stoi(argv[3]));
    }
}
