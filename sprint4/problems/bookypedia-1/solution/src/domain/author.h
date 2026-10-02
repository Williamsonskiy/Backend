// src/domain/author.h
#pragma once
#include <string>
#include <vector>

#include "../util/tagged_uuid.h"
#include "book.h"

namespace domain {

namespace detail {
struct AuthorTag {};
}  

using AuthorId = util::TaggedUUID<detail::AuthorTag>;

class Author {
public:
    Author(AuthorId id, std::string name)
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const AuthorId& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

private:
    AuthorId id_;
    std::string name_;
};

class AuthorRepository {
public:
    virtual void Save(const Author& author) = 0;
    virtual std::vector<Author> GetAuthors() const = 0;

    virtual void SaveBook(const Book& book) = 0;
    virtual std::vector<Book> GetBooks() const = 0;
    virtual std::vector<Book> GetAuthorBooks(const std::string& author_id) const = 0;

protected:
    ~AuthorRepository() = default;
};

}  // namespace domain
