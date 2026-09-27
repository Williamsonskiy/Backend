cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
else()
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
endif()

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
        src/request_handler.h
        src/request_handler.cpp
        src/api_handler.h
        src/app.h
        src/ticker.h
        src/logger.h
        src/loot_generator.h
        src/loot_generator.cpp
)

target_link_libraries(game_server PRIVATE
        ${CONAN_LIBS}
        Threads::Threads
        dl
        rt
        pthread
)

# Тестовый исполняемый файл для Catch2
add_executable(game_server_tests
        src/loot_generator.h
        src/loot_generator.cpp
        tests/main.cpp
        tests/loot_generator_tests.cpp
)
target_include_directories(game_server_tests PRIVATE src ${CONAN_INCLUDE_DIRS})
target_link_libraries(game_server_tests PRIVATE ${CONAN_LIBS} Threads::Threads dl rt pthread)
