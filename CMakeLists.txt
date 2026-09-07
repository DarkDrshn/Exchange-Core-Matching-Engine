cmake_minimum_required(VERSION 3.20)
project(exchange_core VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(exchange_core STATIC
    src/domain/order_book.cpp
    src/engine/matching_engine.cpp
)

target_include_directories(exchange_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(exchange_core PUBLIC cxx_std_17)

enable_testing()

add_executable(exchange_core_tests
    tests/compatibility_tests.cpp
)
target_link_libraries(exchange_core_tests PRIVATE exchange_core)
add_test(NAME exchange_core_compatibility COMMAND exchange_core_tests)
