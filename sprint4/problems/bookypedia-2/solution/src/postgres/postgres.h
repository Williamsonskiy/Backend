// src/postgres/postgres.h
#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>
#include <vector>
#include <optional>

#include "../domain/author.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void Save(const domain::Author& author) override;
    std::vector<domain::Author> GetAuthors() const override;
    std::optional<domain::Author> GetAuthorByName(const std::string& name) const override;
    void DeleteAuthor(const std::string& id) override;
    void EditAuthor(const std::string& id, const std::string& name) override;

    void SaveBook(const domain::Book& book, const std::vector<std::string>& tags) override;
    void AddBookWithNewAuthor(const domain::Book& book, const domain::Author& author, const std::vector<std::string>& tags) override;
    
    std::vector<domain::BookDto> GetBooks() const override;
    std::vector<domain::BookDto> GetAuthorBooks(const std::string& author_id) const override;
    std::vector<domain::BookDto> GetBooksByTitle(const std::string& title) const override;
    void DeleteBook(const std::string& id) override;
    void EditBook(const std::string& id, const std::string& title, int pub_year, const std::vector<std::string>& tags) override;

private:
    pqxx::connection& connection_;
};

class Database {
public:
    explicit Database(pqxx::connection connection);

    AuthorRepositoryImpl& GetAuthors() & {
        return authors_;
    }

private:
    pqxx::connection connection_;
    AuthorRepositoryImpl authors_{connection_};
};

}  // namespace postgres
