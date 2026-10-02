// src/app/use_cases_impl.cpp
#include "use_cases_impl.h"

#include <stdexcept>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("Author name cannot be empty");
    }
    authors_.Save({AuthorId::New(), name});
}

std::vector<domain::Author> UseCasesImpl::GetAuthors() {
    return authors_.GetAuthors();
}

std::optional<domain::Author> UseCasesImpl::GetAuthorByName(const std::string& name) {
    return authors_.GetAuthorByName(name);
}

void UseCasesImpl::DeleteAuthor(const std::string& id) {
    authors_.DeleteAuthor(id);
}

void UseCasesImpl::EditAuthor(const std::string& id, const std::string& name) {
    if (name.empty()) throw std::invalid_argument("Author name cannot be empty");
    authors_.EditAuthor(id, name);
}

void UseCasesImpl::AddBook(const std::string& title, int publication_year, 
                           std::optional<std::string> author_id, 
                           std::optional<std::string> new_author_name, 
                           const std::vector<std::string>& tags) {
    if (title.empty()) {
        throw std::invalid_argument("Book title cannot be empty");
    }
    if (new_author_name) {
        domain::Author author{domain::AuthorId::New(), *new_author_name};
        domain::Book book{domain::BookId::New(), author.GetId().ToString(), title, publication_year};
        authors_.AddBookWithNewAuthor(book, author, tags);
    } else if (author_id) {
        domain::Book book{domain::BookId::New(), *author_id, title, publication_year};
        authors_.SaveBook(book, tags);
    } else {
        throw std::invalid_argument("Author is required");
    }
}

std::vector<domain::BookDto> UseCasesImpl::GetBooks() {
    return authors_.GetBooks();
}

std::vector<domain::BookDto> UseCasesImpl::GetAuthorBooks(const std::string& author_id) {
    return authors_.GetAuthorBooks(author_id);
}

std::vector<domain::BookDto> UseCasesImpl::GetBooksByTitle(const std::string& title) {
    return authors_.GetBooksByTitle(title);
}

void UseCasesImpl::DeleteBook(const std::string& id) {
    authors_.DeleteBook(id);
}

void UseCasesImpl::EditBook(const std::string& id, const std::string& title, int pub_year, const std::vector<std::string>& tags) {
    if (title.empty()) throw std::invalid_argument("Book title cannot be empty");
    authors_.EditBook(id, title, pub_year, tags);
}

}  // namespace app
