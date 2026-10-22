cmake_minimum_required(VERSION 3.16)
project(fcitx5-amar-bangla-kotha)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Fcitx5Core 5.0.0 REQUIRED)
find_package(Fcitx5Module REQUIRED COMPONENTS StandardPath)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

set(SRCS
    abk_fcitx5.cpp
)

add_library(amar-bangla-kotha MODULE ${SRCS})

target_link_libraries(amar-bangla-kotha PRIVATE
    Fcitx5::Core
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../lib/libabk.so
)

set_target_properties(amar-bangla-kotha PROPERTIES
    PREFIX ""
    OUTPUT_NAME "amar-bangla-kotha"
)

# Configuration files
configure_file(amar-bangla-kotha-addon.conf.in ${CMAKE_CURRENT_BINARY_DIR}/amar-bangla-kotha.conf)

install(TARGETS amar-bangla-kotha DESTINATION "${FCITX_INSTALL_ADDONDIR}")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/amar-bangla-kotha.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")
install(FILES amar-bangla-kotha.conf DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod")
