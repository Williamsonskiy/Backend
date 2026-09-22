#pragma once

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/date_time/posix_time/posix_time.hpp> // <-- ДОБАВЛЕНО
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

    // ИСПРАВЛЕНИЕ: правильное извлечение и конвертация времени
    auto ts = logging::extract<boost::posix_time::ptime>("TimeStamp", rec);
    if (ts) {
        root["timestamp"] = boost::posix_time::to_iso_extended_string(*ts);
    } else {
        root["timestamp"] = "";
    }

    root["message"] = *rec[logging::expressions::smessage];

    if (auto data_val = rec[additional_data]) {
        root["data"] = *data_val;
    } else {
        root["data"] = json::object{};
    }

    strm << json::serialize(root);
}

// ... остальной код (InitBoostLogFilter, LogServerStarted и т.д.) без изменений ...
