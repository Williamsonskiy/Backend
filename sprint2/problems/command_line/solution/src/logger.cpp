#include "logger.h"

#include <boost/date_time.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>

#include <iostream>
#include <string>

namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

void JsonFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
    json::object result;

    auto ts = rec[timestamp];
    if (ts) {
        result["timestamp"] = to_iso_extended_string(*ts);
    }

    auto message = rec[expr::smessage];
    result["message"] = message ? message.get() : "";

    auto data = rec[additional_data];
    if (data) {
        result["data"] = *data;
    } else {
        result["data"] = json::object{};
    }

    strm << json::serialize(result);
}

void InitLogging() {
    logging::add_common_attributes();

    logging::add_console_log(
        std::cout,
        keywords::format = &JsonFormatter,
        keywords::auto_flush = true
    );
}

void LogInfo(std::string_view message, json::value data) {
    BOOST_LOG_TRIVIAL(info)
        << logging::add_value(additional_data, std::move(data))
        << message;
}

void LogError(boost::system::error_code ec, std::string_view where) {
    json::object data;
    data["code"] = ec.value();
    data["text"] = ec.message();
    data["where"] = std::string(where);

    LogInfo("error", data);
}
