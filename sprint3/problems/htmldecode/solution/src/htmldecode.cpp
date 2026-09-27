#include "htmldecode.h"

#include <array>
#include <string>
#include <string_view>

namespace {

struct Entity {
    std::string_view name;
    char decoded_char;
};

constexpr std::array<Entity, 10> ENTITIES = {{
    {"lt", '<'},
    {"LT", '<'},
    {"gt", '>'},
    {"GT", '>'},
    {"amp", '&'},
    {"AMP", '&'},
    {"apos", '\''},
    {"APOS", '\''},
    {"quot", '"'},
    {"QUOT", '"'},
}};

bool StartsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size()
        && text.substr(0, prefix.size()) == prefix;
}

}  // namespace

std::string HtmlDecode(std::string_view str) {
    std::string result;
    result.reserve(str.size());

    for (size_t position = 0; position < str.size();) {
        if (str[position] != '&') {
            result.push_back(str[position]);
            ++position;
            continue;
        }

        bool entity_found = false;
        const std::string_view after_ampersand = str.substr(position + 1);

        for (const Entity& entity : ENTITIES) {
            if (StartsWith(after_ampersand, entity.name)) {
                result.push_back(entity.decoded_char);

                position += 1 + entity.name.size();

                if (position < str.size() && str[position] == ';') {
                    ++position;
                }

                entity_found = true;
                break;
            }
        }

        if (!entity_found) {
            result.push_back(str[position]);
            ++position;
        }
    }

    return result;
}
