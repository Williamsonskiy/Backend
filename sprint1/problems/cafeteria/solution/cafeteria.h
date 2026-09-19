#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <atomic>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <chrono>
#include <memory>
#include <mutex>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;
namespace sys = boost::system;

using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

class Order : public std::enable_shared_from_this<Order> {
public:
    Order(net::io_context& io, int id, std::shared_ptr<GasCooker> cooker,
          std::shared_ptr<Sausage> sausage, std::shared_ptr<Bread> bread,
          HotDogHandler handler)
        : id_{id}
        , cooker_{std::move(cooker)}
        , sausage_{std::move(sausage)}
        , bread_{std::move(bread)}
        , handler_{std::move(handler)}
        , sausage_timer_{io}
        , bread_timer_{io}
        , strand_{net::make_strand(io)} {
    }

    void Execute() {
        sausage_->StartFry(*cooker_, [self = shared_from_this()] {
            self->sausage_timer_.expires_after(std::chrono::milliseconds(1500));
            self->sausage_timer_.async_wait(net::bind_executor(self->strand_, [self](sys::error_code ec) {
                self->OnSausageCooked(ec);
            }));
        });
        bread_->StartBake(*cooker_, [self = shared_from_this()] {
            self->bread_timer_.expires_after(std::chrono::milliseconds(1000));
            self->bread_timer_.async_wait(net::bind_executor(self->strand_, [self](sys::error_code ec) {
                self->OnBreadCooked(ec);
            }));
        });
    }

private:
    void OnSausageCooked(sys::error_code ec) {
        if (ec) return HandleError(ec);
        try {
            sausage_->StopFry();
            sausage_ready_ = true;
            CheckReady();
        } catch (...) {
            HandleError(std::current_exception());
        }
    }

    void OnBreadCooked(sys::error_code ec) {
        if (ec) return HandleError(ec);
        try {
            bread_->StopBaking();
            bread_ready_ = true;
            CheckReady();
        } catch (...) {
            HandleError(std::current_exception());
        }
    }

    void CheckReady() {
        if (sausage_ready_ && bread_ready_) {
            try {
                handler_(Result<HotDog>{HotDog{id_, sausage_, bread_}});
            } catch (...) {
                HandleError(std::current_exception());
            }
        }
    }

    void HandleError(std::exception_ptr ptr) {
        if (!error_handled_) {
            error_handled_ = true;
            handler_(Result<HotDog>{ptr});
        }
    }

    void HandleError(sys::error_code ec) {
        HandleError(std::make_exception_ptr(std::runtime_error(ec.message())));
    }

    int id_;
    std::shared_ptr<GasCooker> cooker_;
    std::shared_ptr<Sausage> sausage_;
    std::shared_ptr<Bread> bread_;
    HotDogHandler handler_;
    net::steady_timer sausage_timer_;
    net::steady_timer bread_timer_;
    net::strand<net::io_context::executor_type> strand_;
    bool sausage_ready_ = false;
    bool bread_ready_ = false;
    bool error_handled_ = false;
};

class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io} {
    }

    void OrderHotDog(HotDogHandler handler) {
        std::shared_ptr<Sausage> sausage;
        std::shared_ptr<Bread> bread;
        {
            std::lock_guard<std::mutex> lock(store_mutex_);
            sausage = store_.GetSausage();
            bread = store_.GetBread();
        }
        std::make_shared<Order>(io_, ++next_order_id_, gas_cooker_, std::move(sausage), std::move(bread), std::move(handler))->Execute();
    }

private:
    net::io_context& io_;
    Store store_;
    std::mutex store_mutex_;
    std::atomic<int> next_order_id_{0};
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
};
