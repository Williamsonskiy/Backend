#include <iostream>
#include <string>
#include <optional>
#include <pqxx/pqxx>
#include <boost/json.hpp>

using namespace std;
namespace json = boost::json;

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        return 1;
    }
    
    try {
        pqxx::connection conn(argv[1]);
        
        {
            pqxx::work w(conn);
            w.exec(
                "CREATE TABLE IF NOT EXISTS books ("
                "id SERIAL PRIMARY KEY, "
                "title VARCHAR(100) NOT NULL, "
                "author VARCHAR(100) NOT NULL, "
                "year INTEGER NOT NULL, "
                "ISBN CHAR(13) UNIQUE"
                ");"
            );
            w.commit();
        }

        string line;
        while (getline(cin, line)) {
            if (line.empty()) {
                continue;
            }
            
            json::value jv = json::parse(line);
            string action = jv.as_object().at("action").as_string().c_str();

            if (action == "exit") {
                break;
            } else if (action == "add_book") {
                auto& payload = jv.as_object().at("payload").as_object();
                string title = payload.at("title").as_string().c_str();
                string author = payload.at("author").as_string().c_str();
                int year = payload.at("year").as_int64();
                
                optional<string> isbn;
                if (!payload.at("ISBN").is_null()) {
                    isbn = payload.at("ISBN").as_string().c_str();
                }

                bool success = false;
                try {
                    pqxx::work w(conn);
                    w.exec_params(
                        "INSERT INTO books (title, author, year, ISBN) VALUES ($1, $2, $3, $4)",
                        title, author, year, isbn
                    );
                    w.commit();
                    success = true;
                } catch (const pqxx::sql_error&) {
                    success = false;
                }
                
                json::object resp;
                resp["result"] = success;
                cout << json::serialize(resp) << "\n" << flush;

            } else if (action == "all_books") {
                pqxx::read_transaction r(conn);
                auto res = r.exec("SELECT id, title, author, year, ISBN FROM books ORDER BY year DESC, title ASC, author ASC, ISBN ASC");
                
                json::array arr;
                for (auto row : res) {
                    json::object book;
                    book["id"] = row[0].as<int>();
                    book["title"] = row[1].as<string>();
                    book["author"] = row[2].as<string>();
                    book["year"] = row[3].as<int>();
                    if (row[4].is_null()) {
                        book["ISBN"] = nullptr;
                    } else {
                        book["ISBN"] = row[4].as<string>();
                    }
                    arr.push_back(book);
                }
                cout << json::serialize(arr) << "\n" << flush;
            }
        }
    } catch (const std::exception& e) {
        return 1;
    }
    
    return 0;
}
