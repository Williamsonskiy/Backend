#define BOOST_TEST_MODULE urldecode tests
#include <boost/test/unit_test.hpp>

#include "../src/urldecode.h"

BOOST_AUTO_TEST_CASE(UrlDecode_tests) {
    using namespace std::literals;

    // Пустая строка
    BOOST_TEST(UrlDecode(""sv) == ""s);
    
    // Строка без %-последовательностей
    BOOST_TEST(UrlDecode("HelloWorld"sv) == "HelloWorld"s);
    
    // Строка с символом +
    BOOST_TEST(UrlDecode("Hello+World"sv) == "Hello World"s);
    BOOST_TEST(UrlDecode("+++"sv) == "   "s);
    
    // Строка с валидными %-последовательностями, записанными в разном регистре
    BOOST_TEST(UrlDecode("%20"sv) == " "s);
    BOOST_TEST(UrlDecode("Hello%20World"sv) == "Hello World"s);
    BOOST_TEST(UrlDecode("%21%23%24%26%27%28%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D"sv) == "!#$&'()*+,/:;=?@[]"s);
    BOOST_TEST(UrlDecode("%2a"sv) == "*"s);
    BOOST_TEST(UrlDecode("%2A"sv) == "*"s);
    BOOST_TEST(UrlDecode("%4A%4a"sv) == "JJ"s);
    BOOST_TEST(UrlDecode("%00"sv) == "\0"s); // Проверка символа с кодом 0
    
    // Строка с незакодированными зарезервированными или прочими символами
    BOOST_TEST(UrlDecode("Hello%20World !"sv) == "Hello World !"s);

    // Строка с невалидными %-последовательностями
    BOOST_CHECK_THROW(UrlDecode("%2G"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%G2"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%  "sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%--"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("Hello%2GWorld"sv), std::invalid_argument);
    
    // Строка с неполными %-последовательностями
    BOOST_CHECK_THROW(UrlDecode("%"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("%2"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("Hello%"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("Hello%2"sv), std::invalid_argument);
}
