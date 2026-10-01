#include <iostream>
#include <string>
#include <optional>

#include <pqxx/pqxx>
#include <boost/json/src.hpp> 

using namespace std::literals;
namespace json = boost::json;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <db_connection_string>\n";
        return EXIT_FAILURE;
    }

    try {
        pqxx::connection conn{argv[1]};

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
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;

            json::value jv;
            try {
                jv = json::parse(line);
            } catch (const std::exception&) {
                continue;
            }

            const auto& obj = jv.as_object();
            std::string action(obj.at("action").as_string());

            if (action == "exit") {
                break;
            } else if (action == "add_book") {
                const auto& payload = obj.at("payload").as_object();
                std::string title(payload.at("title").as_string());
                std::string author(payload.at("author").as_string());
                int year = static_cast<int>(payload.at("year").as_int64());
                
                std::optional<std::string> isbn;
                if (!payload.at("ISBN").is_null()) {
                    isbn = std::string(payload.at("ISBN").as_string());
                }

                try {
                    pqxx::work w(conn);
                    w.exec_params(
                        "INSERT INTO books (title, author, year, ISBN) VALUES ($1, $2, $3, $4)",
                        title, author, year, isbn
                    );
                    w.commit();
                    std::cout << json::serialize(json::value{{"result", true}}) << std::endl;
                } catch (const pqxx::sql_error&) {
                    std::cout << json::serialize(json::value{{"result", false}}) << std::endl;
                } catch (const std::exception&) {
                    std::cout << json::serialize(json::value{{"result", false}}) << std::endl;
                }
            } else if (action == "all_books") {
                pqxx::read_transaction r(conn);
                auto res = r.exec(
                    "SELECT id, title, author, year, ISBN FROM books "
                    "ORDER BY year DESC, title ASC, author ASC, ISBN ASC"
                );
                
                json::array arr;
                for (const auto& row : res) {
                    json::object book;
                    book["id"] = row["id"].as<int>();
                    book["title"] = row["title"].c_str();
                    book["author"] = row["author"].c_str();
                    book["year"] = row["year"].as<int>();
                    
                    if (row["ISBN"].is_null()) {
                        book["ISBN"] = nullptr;
                    } else {
                        book["ISBN"] = row["ISBN"].c_str();
                    }
                    arr.push_back(book);
                }
                std::cout << json::serialize(arr) << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
