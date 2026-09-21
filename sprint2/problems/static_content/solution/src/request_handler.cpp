#include "request_handler.h"

#include <string>
#include <cctype>
#include <algorithm>

namespace http_handler {

std::string urlDecode(const std::string& encodedString) {
    std::string decoded;
    decoded.reserve(encodedString.size());

    for (size_t i = 0; i < encodedString.size(); ++i) {
        if (encodedString[i] == '%') {
            if (i + 2 < encodedString.size()) {
                int high = std::isxdigit(static_cast<unsigned char>(encodedString[i + 1])) ?
                    (std::isdigit(static_cast<unsigned char>(encodedString[i + 1])) ? encodedString[i + 1] - '0' :
                            std::tolower(static_cast<unsigned char>(encodedString[i + 1])) - 'a' + 10) : -1;
                int low = std::isxdigit(static_cast<unsigned char>(encodedString[i + 2])) ?
                    (std::isdigit(static_cast<unsigned char>(encodedString[i + 2])) ? encodedString[i + 2] - '0' :
                            std::tolower(static_cast<unsigned char>(encodedString[i + 2])) - 'a' + 10) : -1;

                if (high != -1 && low != -1) {
                    char decodedChar = static_cast<char>((high << 4) | low);
                    decoded += decodedChar;
                    i += 2;
                    continue;
                }
            }
            decoded += encodedString[i];
        } else {
            decoded.push_back(encodedString[i] == '+' ? ' ' : encodedString[i]);
        }
    }
    return decoded;
}

using namespace std::literals;
namespace fs = std::filesystem;

// ИСПРАВЛЕНО: Безопасная проверка подкаталога, которая не ломается об слэши на конце
bool IsSubPath(fs::path path, fs::path base) {
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);

    auto b = base.begin();
    auto p = path.begin();

    while (b != base.end() && p != path.end()) {
        if (*b != *p) {
            break;
        }
        ++b;
        ++p;
    }

    if (b == base.end()) {
        return true;
    }

    // Если base заканчивается на слэш, его последний итератор будет указывать на пустую строку
    if (b->string().empty() && ++b == base.end()) {
        return true;
    }

    return false;
}

// Таблица соответствий расширений файлов и Content-Type
const std::unordered_map<std::string, std::string> contentTypeMap = {
    {".htm", "text/html"},
    {".html", "text/html"},
    {".css", "text/css"},
    {".txt", "text/plain"},
    {".js", "text/javascript"},
    {".json", "application/json"},
    {".xml", "application/xml"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpe", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".bmp", "image/bmp"},
    {".ico", "image/vnd.microsoft.icon"},
    {".tiff", "image/tiff"},
    {".tif", "image/tiff"},
    {".svg", "image/svg+xml"},
    {".svgz", "image/svg+xml"},
    {".mp3", "audio/mpeg"}
};

std::string getContentType(const fs::path& filePath) {
    std::string extension = filePath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c){ return std::tolower(c); });

    auto it = contentTypeMap.find(extension);
    if (it != contentTypeMap.end()) {
        return it->second;
    }

    return "application/octet-stream";
}

void SetIdAndName(boost::json::object& json_map, const model::Map* map){
    json_map["id"] = *(map->GetId());
    json_map["name"] = map->GetName();
}

void SetRoads(boost::json::object& json_map, const model::Map* map){
    json_map["roads"] = boost::json::array{};
    for (const auto& road : map->GetRoads()) {
        boost::json::object road_json;
        if (road.IsHorizontal()) {
            road_json["x0"] = road.GetStart().x;
            road_json["y0"] = road.GetStart().y;
            road_json["x1"] = road.GetEnd().x;
        } else {
            road_json["x0"] = road.GetStart().x;
            road_json["y0"] = road.GetStart().y;
            road_json["y1"] = road.GetEnd().y;
        }
        json_map["roads"].as_array().push_back(road_json);
    }
}

void SetBuildings(boost::json::object& json_map, const model::Map* map){
    json_map["buildings"] = boost::json::array{};
    for (const auto& building : map->GetBuildings()) {
        const auto& bounds = building.GetBounds();
        boost::json::object building_json = {
            {"x", bounds.position.x},
            {"y", bounds.position.y},
            {"w", bounds.size.width},
            {"h", bounds.size.height}
        };
        json_map["buildings"].as_array().push_back(building_json);
    }
}

void SetOffices(boost::json::object& json_map, const model::Map* map){
    json_map["offices"] = boost::json::array{};
    for (const auto& office : map->GetOffices()) {
        boost::json::object office_json = {
            {"id", *office.GetId()},
            {"x", office.GetPosition().x},
            {"y", office.GetPosition().y},
            {"offsetX", office.GetOffset().dx},
            {"offsetY", office.GetOffset().dy}
        };
        json_map["offices"].as_array().push_back(office_json);
    }
}

}  // namespace http_handler
