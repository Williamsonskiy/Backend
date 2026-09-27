#include "urldecode.h"

#include <charconv>
#include <stdexcept>

std::string UrlDecode(std::string_view str) {
    std::string result;
    // Резервируем память, чтобы избежать лишних аллокаций
    result.reserve(str.length());

    // Лямбда для проверки, является ли символ валидным шестнадцатеричным числом
    auto is_hex = [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    };

    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%') {
            if (i + 2 < str.length()) {
                if (is_hex(str[i + 1]) && is_hex(str[i + 2])) {
                    int value = 0;
                    // Используем .data() + смещение для корректного указания границ диапазона
                    std::from_chars(str.data() + i + 1, str.data() + i + 3, value, 16);
                    result.push_back(static_cast<char>(value));
                    i += 2; // Пропускаем уже обработанные символы
                } else {
                    throw std::invalid_argument("Invalid %-sequence");
                }
            } else {
                throw std::invalid_argument("Incomplete %-sequence");
            }
        } else if (str[i] == '+') {
            result.push_back(' ');
        } else {
            result.push_back(str[i]);
        }
    }

    return result;
}
