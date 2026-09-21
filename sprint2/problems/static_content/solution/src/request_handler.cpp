#include "request_handler.h"
#include <string>
#include <cctype>
#include <algorithm>
#include <unordered_map>

namespace http_handler {

std::string UrlDecode(std::string_view encodedString) {
    std::string decoded;
    decoded.reserve(encodedString.size());

    for (size_t i = 0; i < encodedString.size(); ++i) {
        if (encodedString[i] == '%') {
            if (i + 2 < encodedString.size()) {
                auto hex_to_int = [](char c) -> int {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    return -1;
                };

                int high = hex_to_int(encodedString[i + 1]);
                int low = hex_to_int(encodedString[i + 2]);

                if (high != -1 && low != -1) {
                    char decodedChar = static_cast<char>((high << 4) | low);
                    decoded += decodedChar;
                    i += 2;
                    continue;
                }
            }
            decoded += encodedString[i];
        } else if (encodedString[i] == '+') {
            decoded += ' ';
        } else {
            decoded += encodedString[i];
        }
    }
    return decoded;
}

// Защищенный IsSubPath, который работает на GCC 11
bool IsSubPath(fs::path path, fs::path base) {
    path = fs::weakly_canonical(path);
    base = fs::weakly_canonical(base);
    
    std::string p = path.generic_string();
    std::string b = base.generic_string();
    
    if (!b.empty() && b.back() != '/') {
        b += '/';
    }
    
    return p == base.generic_string() || p.starts_with(b);
}

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

std::string GetContentType(const fs::path& filePath) {
    std::string extension = filePath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return std::tolower(c); });

    auto it = contentTypeMap.find(extension);
    if (it != contentTypeMap.end()) {
        return it->second;
    }

    return "application/octet-stream";
}

void SetIdAndName(boost::json::object& json_map, const model::Map* map) {
    json_map["id"] = *(map->GetId());
    json_map["name"] = map->GetName();
}

void SetRoads(boost::json::object& json_map, const model::Map* map) {
    boost::json::array roads_json;
    for (const auto& road : map->GetRoads()) {
        boost::json::object road_obj;
        road_obj["x0"] = road.GetStart().x;
        road_obj["y0"] = road.GetStart().y;
        if (road.IsHorizontal()) {
            road_obj["x1"] = road.GetEnd().x;
        } else {
            road_obj["y1"] = road.GetEnd().y;
        }
        roads_json.push_back(std::move(road_obj));
    }
    json_map["roads"] = std::move(roads_json);
}

void SetBuildings(boost::json::object& json_map, const model::Map* map) {
    boost::json::array buildings_json;
    for (const auto& building : map->GetBuildings()) {
        const auto& bounds = building.GetBounds();
        boost::json::object building_obj = {
            {"x", bounds.position.x},
            {"y", bounds.position.y},
            {"w", bounds.size.width},
            {"h", bounds.size.height}
        };
        buildings_json.push_back(std::move(building_obj));
    }
    json_map["buildings"] = std::move(buildings_json);
}

void SetOffices(boost::json::object& json_map, const model::Map* map) {
    boost::json::array offices_json;
    for (const auto& office : map->GetOffices()) {
        boost::json::object office_obj = {
            {"id", *office.GetId()},
            {"x", office.GetPosition().x},
            {"y", office.GetPosition().y},
            {"offsetX", office.GetOffset().dx},
            {"offsetY", office.GetOffset().dy}
        };
        offices_json.push_back(std::move(office_obj));
    }
    json_map["offices"] = std::move(offices_json);
}

}  // namespace http_handler
