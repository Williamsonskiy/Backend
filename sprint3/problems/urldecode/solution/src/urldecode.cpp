#include "urldecode.h"

#include <stdexcept>

namespace {

int HexToInt(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }

    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }

    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }

    throw std::invalid_argument("Invalid percent-encoding");
}

char DecodePercentSequence(char first, char second) {
    const int high = HexToInt(first);
    const int low = HexToInt(second);

    return static_cast<char>(high * 16 + low);
}

}  // namespace

std::string UrlDecode(std::string_view str) {
    std::string result;
    result.reserve(str.size());

    for (size_t index = 0; index < str.size(); ++index) {
        const char ch = str[index];

        if (ch == '+') {
            result.push_back(' ');
        } else if (ch == '%') {
            if (index + 2 >= str.size()) {
                throw std::invalid_argument("Incomplete percent-encoding");
            }

            result.push_back(DecodePercentSequence(str[index + 1], str[index + 2]));
            index += 2;
        } else {
            result.push_back(ch);
        }
    }

    return result;
}
