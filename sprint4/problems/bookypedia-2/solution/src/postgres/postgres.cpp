// src/postgres/postgres.cpp
#include "postgres.h"

#include <pqxx/pqxx>
#include <pqxx/zview.hxx>
#include <sstream>

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
    auto result = r.exec("SELECT id, name FROM authors ORDER BY name;"_zv);
    std::vector<domain::Author> authors;
    for (const auto& row : result) {
        authors.emplace_back(domain::AuthorId::FromString(row[0].c_str()), row[1].c_str());
    }
    return authors;
}

std::optional<domain::Author> AuthorRepositoryImpl::GetAuthorByName(const std::string& name) const {
    pqxx::read_transaction r{connection_};
    auto result = r.exec_params("SELECT id, name FROM authors WHERE name = $1"_zv, name);
    if (result.empty()) return std::nullopt;
    return domain::Author{domain::AuthorId::FromString(result[0][0].c_str()), result[0][1].c_str()};
}

void AuthorRepositoryImpl::DeleteAuthor(const std::string& id) {
    pqxx::work w{connection_};
    w.exec_params("DELETE FROM book_tags WHERE book_id IN (SELECT id FROM books WHERE author_id = $1)"_zv, id);
    w.exec_params("DELETE FROM books WHERE author_id = $1"_zv, id);
    auto res = w.exec_params("DELETE FROM authors WHERE id = $1"_zv, id);
    if (res.affected_rows() == 0) {
        throw std::runtime_error("Author not found");
    }
    w.commit();
}

void AuthorRepositoryImpl::EditAuthor(const std::string& id, const std::string& name) {
    pqxx::work w{connection_};
    auto res = w.exec_params("UPDATE authors SET name = $1 WHERE id = $2"_zv, name, id);
    if (res.affected_rows() == 0) {
        throw std::runtime_error("Author not found");
    }
    w.commit();
}

void AuthorRepositoryImpl::SaveBook(const domain::Book& book, const std::vector<std::string>& tags) {
    pqxx::work w{connection_};
    w.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4);
)"_zv,
        book.GetId().ToString(), book.GetAuthorId(), book.GetTitle(), book.GetPublicationYear());
    for (const auto& tag : tags) {
        w.exec_params("INSERT INTO book_tags (book_id, tag) VALUES ($1, $2)"_zv, book.GetId().ToString(), tag);
    }
    w.commit();
}

void AuthorRepositoryImpl::AddBookWithNewAuthor(const domain::Book& book, const domain::Author& author, const std::vector<std::string>& tags) {
    pqxx::work w{connection_};
    w.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
    w.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4);
)"_zv,
        book.GetId().ToString(), book.GetAuthorId(), book.GetTitle(), book.GetPublicationYear());
    for (const auto& tag : tags) {
        w.exec_params("INSERT INTO book_tags (book_id, tag) VALUES ($1, $2)"_zv, book.GetId().ToString(), tag);
    }
    w.commit();
}

std::vector<domain::BookDto> AuthorRepositoryImpl::GetBooks() const {
    pqxx::read_transaction r{connection_};
    auto result = r.exec(R"(
        SELECT b.id, b.title, b.author_id, a.name, b.publication_year,
               (SELECT string_agg(tag, ',') FROM book_tags WHERE book_id = b.id)
        FROM books b
        JOIN authors a ON b.author_id = a.id
        ORDER BY b.title, a.name, b.publication_year
    )"_zv);
    
    std::vector<domain::BookDto> books;
    for (const auto& row : result) {
        domain::BookDto dto{row[0].c_str(), row[1].c_str(), row[2].c_str(), row[3].c_str(), row[4].as<int>(), {}};
        if (!row[5].is_null()) {
            std::string tags_str = row[5].c_str();
            std::stringstream ss(tags_str);
            std::string tag;
            while (std::getline(ss, tag, ',')) {
                dto.tags.push_back(tag);
            }
        }
        books.push_back(std::move(dto));
    }
    return books;
}

std::vector<domain::BookDto> AuthorRepositoryImpl::GetAuthorBooks(const std::string& author_id) const {
    pqxx::read_transaction r{connection_};
    auto result = r.exec_params(R"(
        SELECT b.id, b.title, b.author_id, a.name, b.publication_year,
               (SELECT string_agg(tag, ',') FROM book_tags WHERE book_id = b.id)
        FROM books b
        JOIN authors a ON b.author_id = a.id
        WHERE b.author_id = $1
        ORDER BY b.publication_year, b.title
    )"_zv, author_id);
    
    std::vector<domain::BookDto> books;
    for (const auto& row : result) {
        domain::BookDto dto{row[0].c_str(), row[1].c_str(), row[2].c_str(), row[3].c_str(), row[4].as<int>(), {}};
        books.push_back(std::move(dto));
    }
    return books;
}

std::vector<domain::BookDto> AuthorRepositoryImpl::GetBooksByTitle(const std::string& title) const {
    pqxx::read_transaction r{connection_};
    auto result = r.exec_params(R"(
        SELECT b.id, b.title, b.author_id, a.name, b.publication_year,
               (SELECT string_agg(tag, ',') FROM book_tags WHERE book_id = b.id)
        FROM books b
        JOIN authors a ON b.author_id = a.id
        WHERE b.title = $1
        ORDER BY b.title, a.name, b.publication_year
    )"_zv, title);
    
    std::vector<domain::BookDto> books;
    for (const auto& row : result) {
        domain::BookDto dto{row[0].c_str(), row[1].c_str(), row[2].c_str(), row[3].c_str(), row[4].as<int>(), {}};
        if (!row[5].is_null()) {
            std::string tags_str = row[5].c_str();
            std::stringstream ss(tags_str);
            std::string tag;
            while (std::getline(ss, tag, ',')) {
                dto.tags.push_back(tag);
            }
        }
        books.push_back(std::move(dto));
    }
    return books;
}

void AuthorRepositoryImpl::DeleteBook(const std::string& id) {
    pqxx::work w{connection_};
    w.exec_params("DELETE FROM book_tags WHERE book_id = $1"_zv, id);
    auto res = w.exec_params("DELETE FROM books WHERE id = $1"_zv, id);
    if (res.affected_rows() == 0) {
        throw std::runtime_error("Book not found");
    }
    w.commit();
}

void AuthorRepositoryImpl::EditBook(const std::string& id, const std::string& title, int pub_year, const std::vector<std::string>& tags) {
    pqxx::work w{connection_};
    auto res = w.exec_params("UPDATE books SET title = $1, publication_year = $2 WHERE id = $3"_zv, title, pub_year, id);
    if (res.affected_rows() == 0) {
        throw std::runtime_error("Book not found");
    }
    w.exec_params("DELETE FROM book_tags WHERE book_id = $1"_zv, id);
    for (const auto& tag : tags) {
        w.exec_params("INSERT INTO book_tags (book_id, tag) VALUES ($1, $2)"_zv, id, tag);
    }
    w.commit();
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
    CONSTRAINT book_author_id_fkey FOREIGN KEY (author_id) REFERENCES authors (id) ON DELETE CASCADE
);
)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID NOT NULL,
    tag varchar(30) NOT NULL,
    CONSTRAINT book_tags_book_id_fkey FOREIGN KEY (book_id) REFERENCES books (id) ON DELETE CASCADE
);
)"_zv);
    work.commit();
}

}  // namespace postgres
