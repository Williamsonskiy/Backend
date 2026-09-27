#include "urlencode.h"

#include <string>

namespace {

bool IsReservedSymbol(unsigned char c) {
    const std::string reserved = "!#$&'()*+,/:;=?@[]";
    return reserved.find(static_cast<char>(c)) != std::string::npos;
}

bool ShouldEncode(unsigned char c) {
    return IsReservedSymbol(c) || c < 32 || c >= 128;
}

char ToHexDigit(unsigned int value) {
    const char* digits = "0123456789ABCDEF";
    return digits[value];
}

}  // namespace

std::string UrlEncode(std::string_view str) {
    std::string result;

    for (unsigned char c : str) {
        if (c == ' ') {
            result += '+';
        } else if (ShouldEncode(c)) {
            result += '%';
            result += ToHexDigit(c / 16);
            result += ToHexDigit(c % 16);
        } else {
            result += static_cast<char>(c);
        }
    }

    return result;
}
