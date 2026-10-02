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

void UseCasesImpl::AddBook(const std::string& author_id, const std::string& title, int publication_year) {
    if (title.empty()) {
        throw std::invalid_argument("Book title cannot be empty");
    }
    authors_.SaveBook({BookId::New(), author_id, title, publication_year});
}

std::vector<domain::Book> UseCasesImpl::GetBooks() {
    return authors_.GetBooks();
}

std::vector<domain::Book> UseCasesImpl::GetAuthorBooks(const std::string& author_id) {
    return authors_.GetAuthorBooks(author_id);
}

}  // namespace app
