#include "request_handler.h"

namespace http_handler {

// ИСПРАВЛЕНО: удален весь старый (устаревший) код и добавлена 
// реализация статических вспомогательных методов из класса

std::string RequestHandler::MakeErrorJson(std::string_view code, std::string_view message) {
    json::object obj;
    obj["code"] = std::string(code);
    obj["message"] = std::string(message);
    return json::serialize(obj);
}

http::response<http::string_body> RequestHandler::MakeJsonResponse(
    http::status status,
    std::string body,
    unsigned version,
    bool keep_alive,
    http::verb method,
    std::string_view allow_header) {
    
    http::response<http::string_body> res{status, version};
    res.set(http::field::content_type, std::string(ContentType::APPLICATION_JSON));
    res.set(http::field::cache_control, "no-cache");
    
    if (!allow_header.empty()) {
        res.set(http::field::allow, std::string(allow_header));
    }
    
    res.keep_alive(keep_alive);

    if (method == http::verb::head) {
        res.content_length(body.size());
    } else {
        res.body() = std::move(body);
        res.prepare_payload();
    }
    
    return res;
}

}  // namespace http_handler
