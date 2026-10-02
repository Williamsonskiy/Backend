#include <catch2/catch_test_macros.hpp>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h"

namespace {

struct MockAuthorRepository : domain::AuthorRepository {
    std::vector<domain::Author> saved_authors;
    std::vector<domain::Book> saved_books;

    void Save(const domain::Author& author) override {
        saved_authors.emplace_back(author);
    }

    std::vector<domain::Author> GetAuthors() const override {
        return saved_authors;
    }

    void SaveBook(const domain::Book& book) override {
        saved_books.emplace_back(book);
    }

    std::vector<domain::Book> GetBooks() const override {
        return saved_books;
    }

    std::vector<domain::Book> GetAuthorBooks(const std::string& author_id) const override {
        std::vector<domain::Book> result;
        for (const auto& book : saved_books) {
            if (book.GetAuthorId() == author_id) {
                result.push_back(book);
            }
        }
        return result;
    }
};

struct Fixture {
    MockAuthorRepository authors;
};

}  // namespace

SCENARIO_METHOD(Fixture, "Book Adding") {
    GIVEN("Use cases") {
        app::UseCasesImpl use_cases{authors};

        WHEN("Adding an author") {
            const auto author_name = "Joanne Rowling";
            use_cases.AddAuthor(author_name);

            THEN("author with the specified name is saved to repository") {
                REQUIRE(authors.saved_authors.size() == 1);
                CHECK(authors.saved_authors.at(0).GetName() == author_name);
                CHECK(authors.saved_authors.at(0).GetId() != domain::AuthorId{});
            }
        }
    }
}
