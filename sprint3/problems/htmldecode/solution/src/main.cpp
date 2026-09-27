#include <iostream>
#include <string>

#include "htmldecode.h"

int main() {
    std::string input;
    std::getline(std::cin, input);

    std::cout << HtmlDecode(input) << std::endl;
}
