cmake_minimum_required(VERSION 3.11)

project(urldecode CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

# Основная программа
add_executable(urldecode
    src/main.cpp
    src/urldecode.h
    src/urldecode.cpp
)

# Исполняемый файл с тестами
add_executable(tests
    tests/tests.cpp
    src/urldecode.h
    src/urldecode.cpp
)

# Подключаем Boost к тестам (без этого тесты не скомпилируются)
target_link_libraries(tests PRIVATE CONAN_PKG::boost)

# Включаем поддержку автоматического тестирования (CTest) для CI Практикума
enable_testing()
add_test(NAME tests COMMAND tests)
