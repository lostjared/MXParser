cmake_minimum_required(VERSION 3.10)
project(MXParser VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(MXLex REQUIRED)

add_library(mxparser STATIC
    source/parser.cpp
    source/sym_tab.cpp
)

add_library(MXParser::mxparser ALIAS mxparser)

target_include_directories(mxparser PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_options(mxparser PRIVATE
    -Wall
    -Wextra
    -pedantic
)

add_executable(test-parser test.cpp)

target_compile_options(test-parser PRIVATE
    -Wall
    -Wextra
    -pedantic
)

target_link_libraries(test-parser PRIVATE mxparser MXLex::mxlex)

install(TARGETS mxparser
    EXPORT mxparserTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY mxparser/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mxparser
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT mxparserTargets
    FILE mxparserTargets.cmake
    NAMESPACE mxparser::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mxparser
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/MXParserConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/MXParserConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mxparser
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/MXParserConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/MXParserConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/MXParserConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mxparser
)

