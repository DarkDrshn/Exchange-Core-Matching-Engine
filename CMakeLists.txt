cmake_minimum_required(VERSION 3.25)
project(exchange_core VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EXCHANGE_CORE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

add_library(exchange_core_compile_options INTERFACE)
if(MSVC)
    target_compile_options(exchange_core_compile_options INTERFACE /W4)
    if(EXCHANGE_CORE_WARNINGS_AS_ERRORS)
        target_compile_options(exchange_core_compile_options INTERFACE /WX)
    endif()
else()
    target_compile_options(exchange_core_compile_options INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
    )
    if(EXCHANGE_CORE_WARNINGS_AS_ERRORS)
        target_compile_options(exchange_core_compile_options INTERFACE -Werror)
    endif()
endif()

add_library(exchange_core STATIC
    src/domain/instrument_registry.cpp
    src/domain/order_book.cpp
    src/engine/matching_engine.cpp
    src/risk/risk_manager.cpp
)

target_include_directories(exchange_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(exchange_core PUBLIC cxx_std_17)
target_link_libraries(exchange_core PRIVATE exchange_core_compile_options)

enable_testing()

add_executable(exchange_core_tests
    tests/compatibility_tests.cpp
)
target_link_libraries(exchange_core_tests PRIVATE exchange_core exchange_core_compile_options)
add_test(NAME exchange_core_compatibility COMMAND exchange_core_tests)
