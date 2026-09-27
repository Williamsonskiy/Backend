cmake_minimum_required(VERSION 3.11)

project(game_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

find_package(Threads REQUIRED)

add_definitions(-DBOOST_LOG_STATIC_LINK)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/sdk.h
    src/model.h
    src/model.cpp
    src/tagged.h
    src/boost_json.cpp
    src/json_loader.h
    src/json_loader.cpp
    src/json_serializer.h
    src/json_serializer.cpp
    src/request_handler.cpp
    src/request_handler.h
    src/ticker.h
    src/logger.cpp
    src/logger.h
    src/players.h
    src/player_tokens.h
    src/use_cases.h
    src/loot_generator.h
    src/loot_generator.cpp
    src/extra_data.h
    src/extra_data.cpp
)

target_link_libraries(game_server PRIVATE
    ${CONAN_LIBS}
    Threads::Threads
)
