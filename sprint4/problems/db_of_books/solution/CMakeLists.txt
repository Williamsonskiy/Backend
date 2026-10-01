cmake_minimum_required(VERSION 3.15)
project(book_manager)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})

find_package(libpqxx REQUIRED)
find_package(Boost REQUIRED COMPONENTS json)

add_executable(book_manager main.cpp)

target_link_libraries(book_manager PRIVATE libpqxx::pqxx Boost::json)
