#pragma once

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/json.hpp>
#include <string_view>
#include <optional>
#include <string>

namespace logger {

namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace json = boost::json;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

inline void JsonFormatter(const logging::record_view& rec, logging::basic_formatting_ostream<char>& strm) {
    json::object root;

    // Обязательные поля timestamp и message
    auto ts = logging::extract<boost::posix_time::ptime>("TimeStamp", rec);
    if (ts) {
        root["timestamp"] = boost::posix_time::to_iso_extended_string(*ts);
    } else {
        root["timestamp"] = "";
    }
    
    root["message"] = *rec[logging::expressions::smessage];

    // Поле data (если передано через атрибут AdditionalData)
    if (auto data_val = rec[additional_data]) {
        root["data"] = *data_val;
    } else {
        root["data"] = json::object{};
    }

    strm << json::serialize(root);
}

inline void InitBoostLogFilter() {
    logging::add_common_attributes();
    logging::add_console_log(
        std::clog,
        keywords::format = &JsonFormatter,
        keywords::auto_flush = true // КРИТИЧНО: сбрасываем буфер сразу, чтобы автотесты не получали пустые строки
    );
}

// Лог старта сервера
inline void LogServerStarted(const std::string& address, unsigned int port) {
    json::object data;
    data["address"] = address;
    data["port"] = port;

    BOOST_LOG_TRIVIAL(info) 
        << logging::add_value(additional_data, data) 
        << "server started";
}

// Лог завершения работы сервера
inline void LogServerExited(int code, const std::optional<std::string>& exception_msg = std::nullopt) {
    json::object data;
    data["code"] = code;
    if (exception_msg) {
        data["exception"] = *exception_msg;
    }

    BOOST_LOG_TRIVIAL(info) 
        << logging::add_value(additional_data, data) 
        << "server exited";
}

// Лог входящего запроса
inline void LogRequest(std::string_view ip, std::string_view URI, std::string_view method) {
    json::object data;
    data["ip"] = std::string(ip);
    data["URI"] = std::string(URI);
    data["method"] = std::string(method);

    BOOST_LOG_TRIVIAL(info) 
        << logging::add_value(additional_data, data) 
        << "request received";
}

// Лог отправленного ответа
inline void LogResponse(long long response_time, int code, const std::optional<std::string>& content_type) {
    json::object data;
    data["response_time"] = response_time;
    data["code"] = code;
    if (content_type) {
        data["content_type"] = *content_type;
    } else {
        data["content_type"] = "null";
    }

    BOOST_LOG_TRIVIAL(info) 
        << logging::add_value(additional_data, data) 
        << "response sent";
}

} // namespace logger
