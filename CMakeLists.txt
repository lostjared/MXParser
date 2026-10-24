cmake_minimum_required(VERSION 3.10)
project(MXParser VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(MXLex REQUIRED)
find_packagE(Argz REQUIRED)

add_library(mxparser STATIC
    source/parser.cpp
    source/sym_tab.cpp
    source/ast.cpp
    source/evaluate.cpp
    source/functions.cpp
    source/type.cpp
    source/interpreter.cpp
    source/semantic.cpp
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

add_executable(parser-test parser-test.cpp)
add_executable(parse-expr examples/expr.cpp)

target_compile_options(parser-test PRIVATE -Wall -Wextra -pedantic)
target_compile_options(parse-expr PRIVATE -Wall -Wextra -pedantic)

target_link_libraries(test-parser PRIVATE mxparser MXLex::mxlex Argz::argz)
target_link_libraries(parser-test PRIVATE mxparser MXLex::mxlex Argz::argz)
target_link_libraries(parse-expr PRIVATE mxparser MXLex::mxlex Argz::argz)

install(TARGETS mxparser
    EXPORT mxparserTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY MXParser/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/MXParser
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


include(CTest)
if(BUILD_TESTING)
    add_executable(expression-tests tests/expressions.cpp)
    target_link_libraries(expression-tests PRIVATE mxparser MXLex::mxlex)
    target_compile_options(expression-tests PRIVATE -Wall -Wextra -pedantic)
    add_test(NAME expressions COMMAND expression-tests)
    add_executable(function-tests tests/functions.cpp)
    target_link_libraries(function-tests PRIVATE mxparser MXLex::mxlex)
    target_compile_options(function-tests PRIVATE -Wall -Wextra -pedantic)
    add_test(NAME functions COMMAND function-tests)
    add_executable(type-source-tests tests/types.cpp)
    target_link_libraries(type-source-tests PRIVATE mxparser MXLex::mxlex)
    target_compile_options(type-source-tests PRIVATE -Wall -Wextra -pedantic)
    add_test(NAME types-and-source COMMAND type-source-tests)
    add_executable(declarator-tests tests/declarators.cpp)
    target_link_libraries(declarator-tests PRIVATE mxparser MXLex::mxlex)
    target_compile_options(declarator-tests PRIVATE -Wall -Wextra -pedantic)
    add_test(NAME declarators COMMAND declarator-tests)
    add_executable(semantic-tests tests/semantic.cpp)
    target_link_libraries(semantic-tests PRIVATE mxparser MXLex::mxlex)
    target_compile_options(semantic-tests PRIVATE -Wall -Wextra -pedantic)
    add_test(NAME semantics COMMAND semantic-tests)
    add_executable(statement-tests tests/statements.cpp)
    target_link_libraries(statement-tests PRIVATE mxparser MXLex::mxlex)
    target_compile_options(statement-tests PRIVATE -Wall -Wextra -pedantic)
    add_test(NAME statements COMMAND statement-tests)
    add_executable(aggregate-tests tests/aggregates.cpp)
    target_link_libraries(aggregate-tests PRIVATE mxparser MXLex::mxlex)
    target_compile_options(aggregate-tests PRIVATE -Wall -Wextra -pedantic)
    add_test(NAME aggregates COMMAND aggregate-tests)
    add_test(NAME aggregate-syntax COMMAND parse-expr --syntax-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/aggregates.c)
    add_test(NAME aggregate-check COMMAND parse-expr --check-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/aggregates.c)
    add_executable(lsprintf-tests tests/lsprintf.cpp)
    target_link_libraries(lsprintf-tests PRIVATE mxparser MXLex::mxlex)
    target_compile_options(lsprintf-tests PRIVATE -Wall -Wextra -pedantic)
    add_test(NAME lsprintf COMMAND lsprintf-tests)
    add_test(NAME lsprintf-example COMMAND parse-expr --run-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/lsprintf.c)
    set_tests_properties(lsprintf-example PROPERTIES PASS_REGULAR_EXPRESSION "Hello from MXParser! count=3, average=2.50")
    add_executable(execution-tests tests/execution.cpp)
    target_link_libraries(execution-tests PRIVATE mxparser MXLex::mxlex)
    target_compile_options(execution-tests PRIVATE -Wall -Wextra -pedantic)
    add_test(NAME execution COMMAND execution-tests ${CMAKE_CURRENT_SOURCE_DIR}/examples/subset.c)
    add_test(NAME subset-example-run COMMAND parse-expr --run-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/subset.c)
    set_tests_properties(subset-example-run PROPERTIES PASS_REGULAR_EXPRESSION "Result: 2")
    add_executable(publication-tests tests/publication.cpp)
    target_link_libraries(publication-tests PRIVATE mxparser MXLex::mxlex)
    target_compile_options(publication-tests PRIVATE -Wall -Wextra -pedantic)
    add_test(NAME subset-publication COMMAND publication-tests ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME subset-example-syntax COMMAND parse-expr --syntax-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/subset.c)
    add_test(NAME subset-example-check COMMAND parse-expr --check-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/subset.c)
    find_program(MXPARSER_C99_COMPILER NAMES cc gcc clang DOC "C compiler for C99 subset syntax comparisons")
    if(MXPARSER_C99_COMPILER)
        add_test(NAME execution-c99-comparison COMMAND ${CMAKE_COMMAND}
            "-DCOMPILER=${MXPARSER_C99_COMPILER}"
            "-DPARSER=$<TARGET_FILE:parse-expr>"
            "-DTESTER=$<TARGET_FILE:execution-tests>"
            "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/runtime-comparison"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime-compiler.cmake)
        add_test(NAME subset-c99-comparison COMMAND ${CMAKE_COMMAND}
            "-DCOMPILER=${MXPARSER_C99_COMPILER}"
            "-DPARSER=$<TARGET_FILE:parse-expr>"
            "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/c99-comparison"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/compiler.cmake)
    else()
        message(STATUS "C compiler unavailable: subset-c99-comparison will not be registered")
    endif()
    add_executable(subset-tests tests/subset.cpp)
    target_link_libraries(subset-tests PRIVATE mxparser MXLex::mxlex)
    target_compile_options(subset-tests PRIVATE -Wall -Wextra -pedantic)
    add_test(NAME subset-fixtures COMMAND subset-tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/subset/manifest.tsv)
    add_test(NAME function-example COMMAND parse-expr ${CMAKE_CURRENT_SOURCE_DIR}/examples/functions.c)
    set_tests_properties(function-example PROPERTIES PASS_REGULAR_EXPRESSION "Result: 21")
    add_test(NAME function-syntax COMMAND parse-expr --syntax-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/functions.c)
    add_test(NAME declaration-syntax COMMAND parse-expr --syntax-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/declarations.c)
    add_test(NAME function-check COMMAND parse-expr --check-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/functions.c)
    add_test(NAME declaration-check COMMAND parse-expr --check-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/declarations.c)
    add_test(NAME statement-syntax COMMAND parse-expr --syntax-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/statements.c)
    add_test(NAME statement-check COMMAND parse-expr --check-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/statements.c)
endif()
