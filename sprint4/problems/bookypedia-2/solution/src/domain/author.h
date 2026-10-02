// src/domain/author.h
#pragma once
#include <string>
#include <vector>
#include <optional>

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

struct BookDto {
    std::string id;
    std::string title;
    std::string author_id;
    std::string author_name;
    int publication_year;
    std::vector<std::string> tags;
};

class AuthorRepository {
public:
    virtual void Save(const Author& author) = 0;
    virtual std::vector<Author> GetAuthors() const = 0;
    virtual std::optional<Author> GetAuthorByName(const std::string& name) const = 0;
    virtual void DeleteAuthor(const std::string& id) = 0;
    virtual void EditAuthor(const std::string& id, const std::string& name) = 0;

    virtual void SaveBook(const Book& book, const std::vector<std::string>& tags) = 0;
    virtual void AddBookWithNewAuthor(const Book& book, const Author& author, const std::vector<std::string>& tags) = 0;
    
    virtual std::vector<BookDto> GetBooks() const = 0;
    virtual std::vector<BookDto> GetAuthorBooks(const std::string& author_id) const = 0;
    virtual std::vector<BookDto> GetBooksByTitle(const std::string& title) const = 0;
    virtual void DeleteBook(const std::string& id) = 0;
    virtual void EditBook(const std::string& id, const std::string& title, int pub_year, const std::vector<std::string>& tags) = 0;

protected:
    ~AuthorRepository() = default;
};

}  // namespace domain
