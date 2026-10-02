// src/app/use_cases.h
#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual std::vector<domain::Author> GetAuthors() = 0;
    virtual std::optional<domain::Author> GetAuthorByName(const std::string& name) = 0;
    virtual void DeleteAuthor(const std::string& id) = 0;
    virtual void EditAuthor(const std::string& id, const std::string& name) = 0;

    virtual void AddBook(const std::string& title, int publication_year, 
                         std::optional<std::string> author_id, 
                         std::optional<std::string> new_author_name, 
                         const std::vector<std::string>& tags) = 0;
    virtual std::vector<domain::BookDto> GetBooks() = 0;
    virtual std::vector<domain::BookDto> GetAuthorBooks(const std::string& author_id) = 0;
    virtual std::vector<domain::BookDto> GetBooksByTitle(const std::string& title) = 0;
    virtual void DeleteBook(const std::string& id) = 0;
    virtual void EditBook(const std::string& id, const std::string& title, int pub_year, const std::vector<std::string>& tags) = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app
