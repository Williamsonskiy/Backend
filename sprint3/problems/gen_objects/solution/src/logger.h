#pragma once

#include <boost/json.hpp>
#include <boost/log/expressions/keyword.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/system/error_code.hpp>

#include <string_view>

namespace json = boost::json;
namespace logging = boost::log;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

void InitLogging();

void LogInfo(std::string_view message, json::value data);

void LogError(boost::system::error_code ec, std::string_view where);
