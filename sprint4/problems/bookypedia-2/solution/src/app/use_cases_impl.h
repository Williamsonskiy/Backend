// src/app/use_cases_impl.h
#pragma once
#include "../domain/author_fwd.h"
#include "../domain/author.h"
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    explicit UseCasesImpl(domain::AuthorRepository& authors)
        : authors_{authors} {
    }

    void AddAuthor(const std::string& name) override;
    std::vector<domain::Author> GetAuthors() override;
    std::optional<domain::Author> GetAuthorByName(const std::string& name) override;
    void DeleteAuthor(const std::string& id) override;
    void EditAuthor(const std::string& id, const std::string& name) override;

    void AddBook(const std::string& title, int publication_year, 
                 std::optional<std::string> author_id, 
                 std::optional<std::string> new_author_name, 
                 const std::vector<std::string>& tags) override;
    std::vector<domain::BookDto> GetBooks() override;
    std::vector<domain::BookDto> GetAuthorBooks(const std::string& author_id) override;
    std::vector<domain::BookDto> GetBooksByTitle(const std::string& title) override;
    void DeleteBook(const std::string& id) override;
    void EditBook(const std::string& id, const std::string& title, int pub_year, const std::vector<std::string>& tags) override;

private:
    domain::AuthorRepository& authors_;
};

}  // namespace app
