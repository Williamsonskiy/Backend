cmake_minimum_required(VERSION 3.11)

project(game_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(${CMAKE_BINARY_DIR}/conan_paths.cmake OPTIONAL)

find_package(Boost REQUIRED COMPONENTS system thread json)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/request_handler.cpp
    src/json_loader.cpp
    src/model.cpp
)

target_include_directories(game_server PRIVATE src)

target_link_libraries(game_server PRIVATE
    Boost::system
    Boost::thread
    Boost::json
)
