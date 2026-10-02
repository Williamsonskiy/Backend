// tests/use_case_tests.cpp
#include <catch2/catch_test_macros.hpp>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h"

namespace {

struct MockAuthorRepository : domain::AuthorRepository {
    std::vector<domain::Author> saved_authors;

    void Save(const domain::Author& author) override {
        saved_authors.emplace_back(author);
    }
    std::vector<domain::Author> GetAuthors() const override { return saved_authors; }
    std::optional<domain::Author> GetAuthorByName(const std::string& name) const override {
        for (const auto& a : saved_authors) {
            if (a.GetName() == name) return a;
        }
        return std::nullopt;
    }
    void DeleteAuthor(const std::string& id) override {}
    void EditAuthor(const std::string& id, const std::string& name) override {}

    void SaveBook(const domain::Book& book, const std::vector<std::string>& tags) override {}
    void AddBookWithNewAuthor(const domain::Book& book, const domain::Author& author, const std::vector<std::string>& tags) override {}
    
    std::vector<domain::BookDto> GetBooks() const override { return {}; }
    std::vector<domain::BookDto> GetAuthorBooks(const std::string& author_id) const override { return {}; }
    std::vector<domain::BookDto> GetBooksByTitle(const std::string& title) const override { return {}; }
    void DeleteBook(const std::string& id) override {}
    void EditBook(const std::string& id, const std::string& title, int pub_year, const std::vector<std::string>& tags) override {}
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
