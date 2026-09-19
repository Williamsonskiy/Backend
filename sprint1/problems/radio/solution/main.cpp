#include "audio.h"
#include <boost/asio.hpp>
#include <iostream>
#include <string>

using boost::asio::ip::udp;
using namespace std::chrono_literals;

void StartServer(uint16_t port) {
    Player player(ma_format_u8, 1);
    boost::asio::io_context io_context;
    udp::socket socket(io_context, udp::endpoint(udp::v4(), port));
    
    int frame_size = player.GetFrameSize();
    std::vector<char> recv_buffer(65000 * frame_size);

    while (true) {
        udp::endpoint sender_endpoint;
        size_t length = socket.receive_from(boost::asio::buffer(recv_buffer), sender_endpoint);
        size_t frames = length / frame_size;
        player.PlayBuffer(recv_buffer.data(), frames, 1.5s);
    }
}

void StartClient(uint16_t port) {
    Recorder recorder(ma_format_u8, 1);
    boost::asio::io_context io_context;
    udp::socket socket(io_context, udp::v4());
    
    int frame_size = recorder.GetFrameSize();

    while (true) {
        std::string ip;
        if (!std::getline(std::cin, ip)) {
            break;
        }
        if (ip.empty()) {
            continue;
        }

        auto rec_result = recorder.Record(65000, 1.5s);
        
        boost::system::error_code ec;
        auto address = boost::asio::ip::make_address(ip, ec);
        if (!ec) {
            udp::endpoint endpoint(address, port);
            socket.send_to(boost::asio::buffer(rec_result.data.data(), rec_result.frames * frame_size), endpoint);
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        return 1;
    }

    std::string mode = argv[1];
    uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));

    if (mode == "server") {
        StartServer(port);
    } else if (mode == "client") {
        StartClient(port);
    }

    return 0;
}
