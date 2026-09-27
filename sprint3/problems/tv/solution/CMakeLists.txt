cmake_minimum_required(VERSION 3.11)

project(tv CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

enable_testing()

add_executable(app
    src/main.cpp
    src/tv.h
    src/menu.h
    src/controller.h
)

add_executable(catch_tv_tests
    tests/catch_tv_tests.cpp
    tests/catch_controller_tests.cpp
    src/tv.h
    src/menu.h
    src/controller.h
)

target_link_libraries(catch_tv_tests PRIVATE CONAN_PKG::catch2)

add_test(NAME catch_tv_tests COMMAND catch_tv_tests)
