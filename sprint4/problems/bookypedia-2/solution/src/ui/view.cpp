// src/ui/view.cpp
#include "view.h"

#include <boost/algorithm/string/trim.hpp>
#include <cassert>
#include <iostream>
#include <sstream>
#include <set>
#include <algorithm>
#include <cctype>

#include "../app/use_cases.h"
#include "../menu/menu.h"

using namespace std::literals;
namespace ph = std::placeholders;

namespace ui {
namespace detail {

std::ostream& operator<<(std::ostream& out, const AuthorInfo& author) {
    out << author.name;
    return out;
}

std::ostream& operator<<(std::ostream& out, const BookInfo& book) {
    out << book.title << ", " << book.publication_year;
    return out;
}

}  // namespace detail

template <typename T>
void PrintVector(std::ostream& out, const std::vector<T>& vector) {
    int i = 1;
    for (auto& value : vector) {
        out << i++ << " " << value << std::endl;
    }
}

namespace {
std::vector<std::string> NormalizeTags(const std::string& tags_str) {
    std::vector<std::string> tags;
    std::set<std::string> unique;
    std::stringstream ss(tags_str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        boost::algorithm::trim(item);
        if (item.empty()) continue;
        
        std::string normalized;
        bool in_space = false;
        for (char c : item) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                if (!in_space) {
                    normalized += ' ';
                    in_space = true;
                }
            } else {
                normalized += c;
                in_space = false;
            }
        }
        if (unique.insert(normalized).second) {
            tags.push_back(normalized);
        }
    }
    return tags;
}
}

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output} {
    menu_.AddAction("AddAuthor"s, "name"s, "Adds author"s, std::bind(&View::AddAuthor, this, ph::_1));
    menu_.AddAction("DeleteAuthor"s, "name"s, "Deletes author"s, std::bind(&View::DeleteAuthor, this, ph::_1));
    menu_.AddAction("EditAuthor"s, "name"s, "Edits author"s, std::bind(&View::EditAuthor, this, ph::_1));
    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s, std::bind(&View::AddBook, this, ph::_1));
    menu_.AddAction("ShowBook"s, "title"s, "Shows book"s, std::bind(&View::ShowBook, this, ph::_1));
    menu_.AddAction("DeleteBook"s, "title"s, "Deletes book"s, std::bind(&View::DeleteBook, this, ph::_1));
    menu_.AddAction("EditBook"s, "title"s, "Edits book"s, std::bind(&View::EditBook, this, ph::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s, std::bind(&View::ShowAuthors, this));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s, std::bind(&View::ShowBooks, this));
    menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s, std::bind(&View::ShowAuthorBooks, this));
}

bool View::AddAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        use_cases_.AddAuthor(std::move(name));
    } catch (const std::exception&) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        std::string author_id;
        if (name.empty()) {
            auto id_opt = SelectAuthor();
            if (!id_opt) {
                output_ << "Failed to delete author" << std::endl;
                return true;
            }
            author_id = *id_opt;
        } else {
            auto author = use_cases_.GetAuthorByName(name);
            if (!author) {
                output_ << "Failed to delete author" << std::endl;
                return true;
            }
            author_id = author->GetId().ToString();
        }
        use_cases_.DeleteAuthor(author_id);
    } catch (...) {
        output_ << "Failed to delete author" << std::endl;
    }
    return true;
}

bool View::EditAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        std::string author_id;
        if (name.empty()) {
            auto id_opt = SelectAuthor();
            if (!id_opt) {
                output_ << "Failed to edit author" << std::endl;
                return true;
            }
            author_id = *id_opt;
        } else {
            auto author = use_cases_.GetAuthorByName(name);
            if (!author) {
                output_ << "Failed to edit author" << std::endl;
                return true;
            }
            author_id = author->GetId().ToString();
        }
        
        output_ << "Enter new name:" << std::endl;
        std::string new_name;
        std::getline(input_, new_name);
        boost::algorithm::trim(new_name);
        if (new_name.empty()) {
            output_ << "Failed to edit author" << std::endl;
            return true;
        }
        use_cases_.EditAuthor(author_id, new_name);
    } catch (...) {
        output_ << "Failed to edit author" << std::endl;
    }
    return true;
}

bool View::AddBook(std::istream& cmd_input) const {
    try {
        int pub_year;
        std::string title;
        cmd_input >> pub_year;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        output_ << "Enter author name or empty line to select from list:" << std::endl;
        std::string author_input;
        std::getline(input_, author_input);
        boost::algorithm::trim(author_input);

        std::optional<std::string> author_id;
        std::optional<std::string> new_author_name;

        if (author_input.empty()) {
            author_id = SelectAuthor();
            if (!author_id) {
                output_ << "Failed to add book" << std::endl;
                return true;
            }
        } else {
            auto author = use_cases_.GetAuthorByName(author_input);
            if (author) {
                author_id = author->GetId().ToString();
            } else {
                output_ << "No author found. Do you want to add " << author_input << " (y/n)?" << std::endl;
                std::string ans;
                std::getline(input_, ans);
                boost::algorithm::trim(ans);
                if (ans == "y" || ans == "Y") {
                    new_author_name = author_input;
                } else {
                    output_ << "Failed to add book" << std::endl;
                    return true;
                }
            }
        }

        output_ << "Enter tags (comma separated):" << std::endl;
        std::string tags_line;
        std::getline(input_, tags_line);
        auto tags = NormalizeTags(tags_line);

        use_cases_.AddBook(title, pub_year, author_id, new_author_name, tags);
    } catch (...) {
        output_ << "Failed to add book" << std::endl;
    }
    return true;
}

bool View::ShowBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        auto book = SelectBook(title);
        if (!book) return true;
        
        output_ << "Title: " << book->title << std::endl;
        output_ << "Author: " << book->author_name << std::endl;
        output_ << "Publication year: " << book->publication_year << std::endl;
        if (!book->tags.empty()) {
            output_ << "Tags: ";
            auto sorted_tags = book->tags;
            std::sort(sorted_tags.begin(), sorted_tags.end());
            for (size_t i = 0; i < sorted_tags.size(); ++i) {
                output_ << sorted_tags[i] << (i + 1 == sorted_tags.size() ? "" : ", ");
            }
            output_ << std::endl;
        }
    } catch (...) {}
    return true;
}

bool View::DeleteBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        auto book = SelectBook(title);
        if (!book) {
            output_ << "Failed to delete book" << std::endl;
            return true;
        }
        use_cases_.DeleteBook(book->id);
    } catch (...) {
        output_ << "Failed to delete book" << std::endl;
    }
    return true;
}

bool View::EditBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        auto book = SelectBook(title);
        if (!book) {
            output_ << "Book not found" << std::endl;
            return true;
        }

        output_ << "Enter new title or empty line to use the current one (" << book->title << "):" << std::endl;
        std::string new_title;
        std::getline(input_, new_title);
        boost::algorithm::trim(new_title);
        if (new_title.empty()) new_title = book->title;

        output_ << "Enter publication year or empty line to use the current one (" << book->publication_year << "):" << std::endl;
        std::string pub_year_str;
        std::getline(input_, pub_year_str);
        boost::algorithm::trim(pub_year_str);
        int new_year = book->publication_year;
        if (!pub_year_str.empty()) {
            new_year = std::stoi(pub_year_str);
        }

        output_ << "Enter tags (current tags: ";
        auto tags = book->tags;
        std::sort(tags.begin(), tags.end());
        for (size_t i = 0; i < tags.size(); ++i) {
            output_ << tags[i] << (i + 1 == tags.size() ? "" : ", ");
        }
        output_ << "):" << std::endl;

        std::string tags_str;
        std::getline(input_, tags_str);
        auto new_tags = NormalizeTags(tags_str);
        
        use_cases_.EditBook(book->id, new_title, new_year, new_tags);
    } catch (...) {
        output_ << "Book not found" << std::endl;
    }
    return true;
}

bool View::ShowAuthors() const {
    PrintVector(output_, GetAuthors());
    return true;
}

bool View::ShowBooks() const {
    int i = 1;
    for (const auto& book : use_cases_.GetBooks()) {
        output_ << i++ << " " << book.title << " by " << book.author_name << ", " << book.publication_year << std::endl;
    }
    return true;
}

bool View::ShowAuthorBooks() const {
    try {
        if (auto author_id = SelectAuthor()) {
            PrintVector(output_, GetAuthorBooks(*author_id));
        }
    } catch (...) {
        throw std::runtime_error("Failed to Show Books");
    }
    return true;
}

std::optional<detail::AddBookParams> View::GetBookParams(std::istream& cmd_input) const {
    detail::AddBookParams params;
    cmd_input >> params.publication_year;
    std::getline(cmd_input, params.title);
    boost::algorithm::trim(params.title);

    auto author_id = SelectAuthor();
    if (!author_id.has_value())
        return std::nullopt;
    else {
        params.author_id = author_id.value();
        return params;
    }
}

std::optional<std::string> View::SelectAuthor() const {
    output_ << "Select author:" << std::endl;
    auto authors = GetAuthors();
    PrintVector(output_, authors);
    output_ << "Enter author # or empty line to cancel" << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }

    int author_idx;
    try {
        author_idx = std::stoi(str);
    } catch (std::exception const&) {
        throw std::runtime_error("Invalid author num");
    }

    --author_idx;
    if (author_idx < 0 or author_idx >= authors.size()) {
        throw std::runtime_error("Invalid author num");
    }

    return authors[author_idx].id;
}

std::optional<domain::BookDto> View::SelectBook(const std::string& title_hint) const {
    auto books = title_hint.empty() ? use_cases_.GetBooks() : use_cases_.GetBooksByTitle(title_hint);
    if (books.empty()) return std::nullopt;
    if (books.size() == 1) return books.front();
    
    int i = 1;
    for (const auto& book : books) {
        output_ << i++ << " " << book.title << " by " << book.author_name << ", " << book.publication_year << std::endl;
    }
    output_ << "Enter the book # or empty line to cancel:" << std::endl;
    std::string line;
    std::getline(input_, line);
    boost::algorithm::trim(line);
    if (line.empty()) return std::nullopt;
    try {
        int idx = std::stoi(line) - 1;
        if (idx >= 0 && idx < books.size()) return books[idx];
    } catch (...) {}
    return std::nullopt;
}

std::vector<detail::AuthorInfo> View::GetAuthors() const {
    std::vector<detail::AuthorInfo> dst_autors;
    for (const auto& author : use_cases_.GetAuthors()) {
        dst_autors.push_back({author.GetId().ToString(), author.GetName()});
    }
    return dst_autors;
}

std::vector<detail::BookInfo> View::GetBooks() const {
    std::vector<detail::BookInfo> books;
    for (const auto& book : use_cases_.GetBooks()) {
        books.push_back({book.title, book.publication_year});
    }
    return books;
}

std::vector<detail::BookInfo> View::GetAuthorBooks(const std::string& author_id) const {
    std::vector<detail::BookInfo> books;
    for (const auto& book : use_cases_.GetAuthorBooks(author_id)) {
        books.push_back({book.title, book.publication_year});
    }
    return books;
}

}  // namespace ui
