#include "view.h"

#include <boost/algorithm/string/trim.hpp>
#include <cassert>
#include <iostream>
#include <sstream>
#include <vector>
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
        if (std::find(tags.begin(), tags.end(), normalized) == tags.end()) {
            tags.push_back(normalized);
        }
    }
    // Сортировка тегов по алфавиту для корректного сохранения и последующего отображения
    std::sort(tags.begin(), tags.end());
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
    menu_.AddAction("EditBook"s, "title"s, "Edits book"s, std::b
