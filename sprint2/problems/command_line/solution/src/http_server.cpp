#include "http_server.h"

#include <boost/asio/dispatch.hpp>
#include <boost/json.hpp>

#include <chrono>
#include <string>

#include "logger.h"

namespace http_server {

using namespace std::literals;

namespace {
constexpr std::string_view kReadOp = "read"sv;
constexpr std::string_view kWriteOp = "write"sv;
constexpr std::string_view kShutdownOp = "shutdown"sv;
}  // namespace

void ReportError(beast::error_code ec, std::string_view what) {
    LogError(ec, what);
}

SessionBase::SessionBase(tcp::socket&& socket)
    : stream_(std::move(socket)) {
}

void SessionBase::Run() {
    net::dispatch(stream_.get_executor(),
                  beast::bind_front_handler(&SessionBase::Read, GetSharedThis()));
}

void SessionBase::Read() {
    request_ = HttpRequest{};
    stream_.expires_after(std::chrono::seconds(30));

    http::async_read(stream_,
                     buffer_,
                     request_,
                     beast::bind_front_handler(&SessionBase::OnRead, GetSharedThis()));
}

void SessionBase::OnRead(beast::error_code ec, [[maybe_unused]] std::size_t bytes_read) {
    if (ec == http::error::end_of_stream) {
        return Close();
    }

    if (ec) {
        return ReportError(ec, kReadOp);
    }

    request_start_time_ = std::chrono::steady_clock::now();
    LogRequest(request_);

    HandleRequest(std::move(request_));
}

void SessionBase::OnWrite(bool close,
                          beast::error_code ec,
                          [[maybe_unused]] std::size_t bytes_written) {
    if (ec) {
        return ReportError(ec, kWriteOp);
    }

    if (close) {
        return Close();
    }

    Read();
}

void SessionBase::Close() {
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

    if (ec) {
        ReportError(ec, kShutdownOp);
    }
}

void SessionBase::LogRequest(const HttpRequest& request) {
    boost::json::object data;

    beast::error_code ec;
    const auto endpoint = stream_.socket().remote_endpoint(ec);
    data["ip"] = ec ? std::string{} : endpoint.address().to_string();

    data["URI"] = std::string(request.target());
    data["method"] = std::string(request.method_string());

    LogInfo("request received", data);
}

}  // namespace http_server
