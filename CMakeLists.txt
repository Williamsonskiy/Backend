cmake_minimum_required(VERSION 3.11)

project(game_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

find_package(Threads REQUIRED)

add_definitions(-DBOOST_LOG_STATIC_LINK)

add_library(game_model STATIC
    src/model.cpp
    src/model.h
    src/loot_generator.cpp
    src/loot_generator.h
    src/json_loader.cpp
    src/json_loader.h
    src/collision_detector.cpp
    src/collision_detector.h
    src/geom.h
    src/sdk.h
    src/tagged.h
)

target_include_directories(game_model PUBLIC src)

target_link_libraries(game_model PUBLIC
    ${CONAN_LIBS}
    Threads::Threads
)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/boost_json.cpp
    src/request_handler.cpp
    src/request_handler.h
    src/api_handler.h
    src/app.h
    src/ticker.h
    src/logger.h
)

target_link_libraries(game_server PRIVATE
    game_model
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    file(GLOB TEST_SOURCES "tests/*.cpp")
    if(TEST_SOURCES)
        add_executable(game_server_tests ${TEST_SOURCES})
        target_link_libraries(game_server_tests PRIVATE
            game_model
        )
    endif()
endif()
