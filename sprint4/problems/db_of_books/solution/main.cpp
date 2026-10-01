#include <iostream>
#include <string>
#include <optional>
#include <pqxx/pqxx>
#include <boost/json.hpp>

using namespace std::literals;
namespace json = boost::json;

int main(int argc, char* argv[]) {
    // Подключение к БД передается первым параметром командной строки
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <db_connection_string>\n";
        return EXIT_FAILURE;
    }

    try {
        // Устанавливаем соединение с базой данных
        pqxx::connection conn{argv[1]};

        // Создаем таблицу books
        {
            pqxx::work w(conn);
            w.exec(R"(
                CREATE TABLE IF NOT EXISTS books (
                    id SERIAL PRIMARY KEY,
                    title VARCHAR(100) NOT NULL,
                    author VARCHAR(100) NOT NULL,
                    year INTEGER NOT NULL,
                    ISBN CHAR(13) UNIQUE
                );
            )");
            w.commit();
        }

        std::string line;
        // Читаем запросы из стандартного ввода построчно
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;

            json::value jv;
            try {
                jv = json::parse(line);
            } catch (const std::exception&) {
                continue; 
            }

            if (!jv.is_object()) continue;
            const auto& obj = jv.as_object();
            
            if (!obj.contains("action")) continue;
            std::string action = obj.at("action").as_string().c_str();

            if (action == "exit") {
                break;
            } else if (action == "add_book") {
                const auto& payload = obj.at("payload").as_object();
                std::string title = payload.at("title").as_string().c_str();
                std::string author = payload.at("author").as_string().c_str();
                int year = static_cast<int>(payload.at("year").as_int64());
                
                std::optional<std::string> isbn;
                if (!payload.at("ISBN").is_null()) {
                    isbn = std::string(payload.at("ISBN").as_string().c_str());
                }

                try {
                    pqxx::work w(conn);
                    w.exec_params(
                        "INSERT INTO books (title, author, year, ISBN) VALUES ($1, $2, $3, $4)",
                        title, author, year, isbn
                    );
                    w.commit();
                    
                    json::object res;
                    res["result"] = true;
                    std::cout << json::serialize(res) << std::endl;
                } catch (const pqxx::sql_error&) {
                    // Перехват исключения уникальности ISBN (pqxx::sql_error)
                    json::object res;
                    res["result"] = false;
                    std::cout << json::serialize(res) << std::endl;
                } catch (const std::exception&) {
                    json::object res;
                    res["result"] = false;
                    std::cout << json::serialize(res) << std::endl;
                }
            } else if (action == "all_books") {
                // Используем транзакцию чтения (read_transaction)
                pqxx::read_transaction r(conn);
                
                auto res = r.exec(
                    "SELECT id, title, author, year, ISBN FROM books "
                    "ORDER BY year DESC, title ASC, author ASC, ISBN ASC"
                );
                
                json::array arr;
                for (const auto& row : res) {
                    json::object book;
                    book["id"] = row["id"].as<int>();
                    book["title"] = row["title"].as<std::string>();
                    book["author"] = row["author"].as<std::string>();
                    book["year"] = row["year"].as<int>();
                    
                    if (row["ISBN"].is_null()) {
                        book["ISBN"] = nullptr;
                    } else {
                        std::string isbn_val = row["ISBN"].as<std::string>();
                        // БД может добавить пробелы (pad) для CHAR(13). Очищаем их.
                        while(!isbn_val.empty() && isbn_val.back() == ' ') {
                            isbn_val.pop_back();
                        }
                        book["ISBN"] = isbn_val;
                    }
                    arr.push_back(book);
                }
                std::cout << json::serialize(arr) << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Database Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
