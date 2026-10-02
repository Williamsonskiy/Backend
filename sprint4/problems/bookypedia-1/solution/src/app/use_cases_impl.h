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
    void AddBook(const std::string& author_id, const std::string& title, int publication_year) override;
    std::vector<domain::Book> GetBooks() override;
    std::vector<domain::Book> GetAuthorBooks(const std::string& author_id) override;

private:
    domain::AuthorRepository& authors_;
};

}  // namespace app
