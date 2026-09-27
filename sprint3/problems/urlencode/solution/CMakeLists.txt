cmake_minimum_required(VERSION 3.11)

project(urlencode CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

add_executable(urlencode
    src/main.cpp
    src/urlencode.h
    src/urlencode.cpp
)

add_executable(tests
    tests/tests.cpp
    src/urlencode.h
    src/urlencode.cpp
)

target_link_libraries(tests PRIVATE CONAN_PKG::gtest)

add_test(NAME tests COMMAND tests)
