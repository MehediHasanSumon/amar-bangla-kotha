cmake_minimum_required(VERSION 3.15)
project(abk_tsf LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)

set(SOURCES
    dllmain.cpp
    tsf_engine.cpp
    ../../src/utf8_util.cpp
    ../../src/transliterator.cpp
    ../../src/trie.cpp
    ../../src/dictionary.cpp
    ../../src/autocorrect.cpp
    ../../src/engine.cpp
    ../../src/c_api.cpp
)

add_library(abk_tsf SHARED ${SOURCES} abk_tsf.def)

target_link_libraries(abk_tsf PRIVATE
    ole32
    oleaut32
    uuid
    shlwapi
    advapi32
)

target_compile_definitions(abk_tsf PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

set_target_properties(abk_tsf PROPERTIES
    OUTPUT_NAME "abk_tsf"
    PREFIX ""
)
