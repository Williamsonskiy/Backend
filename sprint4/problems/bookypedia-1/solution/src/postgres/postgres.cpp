// src/postgres/postgres.cpp
#include "postgres.h"

#include <pqxx/zview.hxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
    work.commit();
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAuthors() const {
    pqxx::read_transaction r{connection_};
    auto result = r.query<std::string, std::string>("SELECT id, name FROM authors ORDER BY name;"_zv);
    std::vector<domain::Author> authors;
    for (const auto& [id_str, name] : result) {
        authors.emplace_back(domain::AuthorId::FromString(id_str), name);
    }
    return authors;
}

void AuthorRepositoryImpl::SaveBook(const domain::Book& book) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4);
)"_zv,
        book.GetId().ToString(), book.GetAuthorId(), book.GetTitle(), book.GetPublicationYear());
    work.commit();
}

std::vector<domain::Book> AuthorRepositoryImpl::GetBooks() const {
    pqxx::read_transaction r{connection_};
    auto result = r.query<std::string, std::string, std::string, int>("SELECT id, author_id, title, publication_year FROM books ORDER BY title;"_zv);
    std::vector<domain::Book> books;
    for (const auto& [id_str, author_id_str, title, publication_year] : result) {
        books.emplace_back(domain::BookId::FromString(id_str), author_id_str, title, publication_year);
    }
    return books;
}

std::vector<domain::Book> AuthorRepositoryImpl::GetAuthorBooks(const std::string& author_id) const {
    pqxx::read_transaction r{connection_};
    auto result = r.exec_params("SELECT id, author_id, title, publication_year FROM books WHERE author_id = $1 ORDER BY publication_year, title;"_zv, author_id);
    std::vector<domain::Book> books;
    for (const auto& row : result) {
        books.emplace_back(domain::BookId::FromString(row[0].as<std::string>()), row[1].as<std::string>(), row[2].as<std::string>(), row[3].as<int>());
    }
    return books;
}

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work work{connection_};
    work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID CONSTRAINT book_id_constraint PRIMARY KEY,
    author_id UUID NOT NULL,
    title varchar(100) NOT NULL,
    publication_year integer,
    CONSTRAINT book_author_id_fkey FOREIGN KEY (author_id) REFERENCES authors (id)
);
)"_zv);
    work.commit();
}

}  // namespace postgres
