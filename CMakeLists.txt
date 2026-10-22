cmake_minimum_required(VERSION 3.16)
project(amar_bangla_kotha VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Compiler warnings and optimization
if(MSVC)
    add_compile_options(/W4 /utf-8 /O2)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O3)
endif()

# Core engine include directory
include_directories(include)

# Core engine sources
set(ABK_ENGINE_SOURCES
    src/utf8_util.cpp
    src/transliterator.cpp
    src/trie.cpp
    src/dictionary.cpp
    src/autocorrect.cpp
    src/engine.cpp
    src/c_api.cpp
)

# Static and Shared Library
add_library(abk_static STATIC ${ABK_ENGINE_SOURCES})
set_target_properties(abk_static PROPERTIES OUTPUT_NAME "abk")

add_library(abk_shared SHARED ${ABK_ENGINE_SOURCES})
set_target_properties(abk_shared PROPERTIES OUTPUT_NAME "abk")

# CLI Interactive Application
add_executable(abk_cli cli/main.cpp)
target_link_libraries(abk_cli PRIVATE abk_static)

# Unit Test Runner (Catch2)
add_executable(abk_tests tests/catch_main.cpp tests/test_engine.cpp)
target_include_directories(abk_tests PRIVATE tests)
target_link_libraries(abk_tests PRIVATE abk_static)

# Benchmark Runner
add_executable(abk_benchmark tests/benchmark.cpp)
target_link_libraries(abk_benchmark PRIVATE abk_static)

# Fuzz Test Runner
add_executable(abk_fuzz tests/fuzz_test.cpp)
target_link_libraries(abk_fuzz PRIVATE abk_static)

# Installation targets
install(TARGETS abk_static abk_shared abk_cli
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(DIRECTORY include/abk DESTINATION include)
