cmake_minimum_required(VERSION 3.11)

project(game_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(${CMAKE_BINARY_DIR}/conan_paths.cmake OPTIONAL)

# Добавлена зависимость Threads, она нужна для корректной работы Boost.Thread
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread json log log_setup program_options)

# КРИТИЧЕСКИ ВАЖНО: без этого макроса Boost.Log падает на старте сервера при статической линковке
add_definitions(-DBOOST_LOG_STATIC_LINK)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/request_handler.cpp
    src/json_loader.cpp
    src/model.cpp
)

target_include_directories(game_server PRIVATE src)

target_link_libraries(game_server PRIVATE
    Threads::Threads
    Boost::system
    Boost::thread
    Boost::json
    Boost::log
    Boost::log_setup
    Boost::program_options
)
