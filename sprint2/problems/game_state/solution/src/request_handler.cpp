#include "request_handler.h"
#include <cctype>
#include <algorithm>
#include <unordered_map>

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
        } else if (encodedString[i] == '+') {
            decoded += ' ';
        } else {
            decoded += encodedString[i];
        }
    }
    return decoded;
}

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
    {".htm", "text/html"}, {".html", "text/html"}, {".css", "text/css"},
    {".txt", "text/plain"}, {".js", "text/javascript"}, {".json", "application/json"},
    {".xml", "application/xml"}, {".png", "image/png"}, {".jpg", "image/jpeg"},
    {".jpe", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".gif", "image/gif"},
    {".bmp", "image/bmp"}, {".ico", "image/vnd.microsoft.icon"}, {".tiff", "image/tiff"},
    {".tif", "image/tiff"}, {".svg", "image/svg+xml"}, {".svgz", "image/svg+xml"},
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

} // namespace http_handler
