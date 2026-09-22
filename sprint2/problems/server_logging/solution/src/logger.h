#pragma once

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/keywords/format.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <iostream>
#include <string_view>

namespace logger {

namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace json = boost::json;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)
BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

inline void JsonFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    json::object log_obj;

    if (auto ts = rec[timestamp]) {
        log_obj["timestamp"] = boost::posix_time::to_iso_extended_string(*ts);
    }

    if (auto data = rec[additional_data]) {
        log_obj["data"] = *data;
    }

    log_obj["message"] = *rec[logging::expressions::smessage];

    strm << json::serialize(log_obj);
}

inline void InitLogger() {
    logging::add_common_attributes();

    logging::add_console_log(
        std::clog,
        keywords::format = &JsonFormatter,
        keywords::auto_flush = true
    );
}

inline void LogServerStarted(int port, const std::string& address) {
    json::value data{
        {"port", port},
        {"address", address}
    };
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data)
                            << "server started";
}

inline void LogServerExited(int code, const std::optional<std::string>& exception_msg = std::nullopt) {
    json::object data_obj;
    data_obj["code"] = code;
    if (exception_msg) {
        data_obj["exception"] = *exception_msg;
    }
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, json::value(data_obj))
                            << "server exited";
}

inline void LogNetworkError(int code, std::string_view text, std::string_view where) {
    json::value data{
        {"code", code},
        {"text", std::string(text)},
        {"where", std::string(where)}
    };
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data)
                            << "error";
}

inline void LogRequest(std::string_view ip, std::string_view uri, std::string_view method) {
    json::value data{
        {"ip", std::string(ip)},
        {"URI", std::string(uri)},
        {"method", std::string(method)}
    };
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, data)
                            << "request received";
}

inline void LogResponse(std::string_view ip, long long response_time, int code, const std::optional<std::string>& content_type) {
    json::object data_obj;
    data_obj["ip"] = std::string(ip);
    data_obj["response_time"] = response_time;
    data_obj["code"] = code;
    if (content_type) {
        data_obj["content_type"] = *content_type;
    } else {
        data_obj["content_type"] = nullptr;
    }
    BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, json::value(data_obj))
                            << "response sent";
}

}  // namespace logger
