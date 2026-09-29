#pragma once

#include <boost/asio/strand.hpp>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <memory>
#include <functional>

namespace ticker {

class Ticker : public std::enable_shared_from_this<Ticker> {
public:
    using Strand = boost::asio::strand<boost::asio::io_context::executor_type>;
    using Handler = std::function<void(std::chrono::milliseconds delta)>;

    Ticker(Strand strand, std::chrono::milliseconds period, Handler handler)
        : strand_{strand}
        , period_{period}
        , handler_{std::move(handler)} 
    {}

    void Start() {
        last_tick_ = std::chrono::steady_clock::now();
        ScheduleTick();
    }

private:
    void ScheduleTick() {
        timer_.expires_after(period_);
        timer_.async_wait([self = shared_from_this()](boost::system::error_code ec) {
            self->OnTick(ec);
        });
    }

    void OnTick(boost::system::error_code ec) {
        if (ec) return;

        auto current_tick = std::chrono::steady_clock::now();
        auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(current_tick - last_tick_);
        last_tick_ = current_tick;

        handler_(delta);
        ScheduleTick();
    }

    Strand strand_;
    std::chrono::milliseconds period_;
    Handler handler_;
    boost::asio::steady_timer timer_{strand_};
    std::chrono::steady_clock::time_point last_tick_;
};

} // namespace ticker
