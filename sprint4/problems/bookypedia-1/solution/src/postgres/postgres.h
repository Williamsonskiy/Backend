// src/postgres/postgres.h
#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>
#include <vector>

#include "../domain/author.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void Save(const domain::Author& author) override;
    std::vector<domain::Author> GetAuthors() const override;

    void SaveBook(const domain::Book& book) override;
    std::vector<domain::Book> GetBooks() const override;
    std::vector<domain::Book> GetAuthorBooks(const std::string& author_id) const override;

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
