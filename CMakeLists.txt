cmake_minimum_required(VERSION 3.10)
project(MXParser VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(mxparser_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic)
    endif()
endfunction()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

find_package(MXLex REQUIRED)
find_packagE(Argz REQUIRED)
option(WITH_NATIVE_MODULES "Enable shared-library imports using libffi" ON)
option(WITH_SDL3_DEMO "Build the optional SDL3 graphics module" OFF)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_library(mxparser STATIC
    source/parser.cpp
    source/sym_tab.cpp
    source/ast.cpp
    source/evaluate.cpp
    source/functions.cpp
    source/type.cpp
    source/interpreter.cpp
    source/native_module.cpp
    source/semantic.cpp
)

add_library(MXParser::mxparser ALIAS mxparser)
target_link_libraries(mxparser PUBLIC MXLex::mxlex)
if(WITH_NATIVE_MODULES)
    find_package(LibFFI REQUIRED)
    target_compile_definitions(mxparser PRIVATE MXPARSER_NATIVE_MODULES)
    target_link_libraries(mxparser PRIVATE LibFFI::LibFFI ${CMAKE_DL_LIBS})
    add_library(native-module-example SHARED examples/module.cpp)
    mxparser_warnings(native-module-example)
endif()

if(WITH_SDL3_DEMO)
    if(NOT WITH_NATIVE_MODULES)
        message(FATAL_ERROR "WITH_SDL3_DEMO requires WITH_NATIVE_MODULES=ON")
    endif()
    find_package(SDL3 3.2 REQUIRED CONFIG)
    add_library(ls-graphics SHARED examples/graphics/graphics.cpp)
    target_compile_definitions(ls-graphics PRIVATE LS_GRAPHICS_BUILD)
    target_link_libraries(ls-graphics PRIVATE SDL3::SDL3)
    mxparser_warnings(ls-graphics)
endif()

target_include_directories(mxparser PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

mxparser_warnings(mxparser)

add_executable(test-parser test.cpp)

mxparser_warnings(test-parser)

add_executable(parser-test parser-test.cpp)
add_executable(ls-c examples/expr.cpp examples/repl.cpp)
add_executable(native-example examples/native.cpp)
target_link_libraries(native-example PRIVATE mxparser MXLex::mxlex)
mxparser_warnings(native-example)
if(NOT WIN32)
    find_path(READLINE_INCLUDE_DIR readline/readline.h)
    find_library(READLINE_LIBRARY NAMES readline)
    if(NOT READLINE_INCLUDE_DIR OR NOT READLINE_LIBRARY)
        message(FATAL_ERROR "GNU Readline development files are required for ls-c on Unix. Install readline-devel/libreadline-dev or set READLINE_INCLUDE_DIR and READLINE_LIBRARY.")
    endif()
    target_include_directories(ls-c PRIVATE ${READLINE_INCLUDE_DIR})
    target_link_libraries(ls-c PRIVATE ${READLINE_LIBRARY})
endif()

mxparser_warnings(parser-test)
mxparser_warnings(ls-c)

target_link_libraries(test-parser PRIVATE mxparser MXLex::mxlex Argz::argz)
target_link_libraries(parser-test PRIVATE mxparser MXLex::mxlex Argz::argz)
target_link_libraries(ls-c PRIVATE mxparser MXLex::mxlex Argz::argz)

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
install(FILES cmake/FindLibFFI.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mxparser)


include(CTest)
if(BUILD_TESTING)
    if(WITH_SDL3_DEMO)
        add_executable(graphics-tests tests/graphics.cpp)
        target_link_libraries(graphics-tests PRIVATE mxparser SDL3::SDL3)
        mxparser_warnings(graphics-tests)
        add_test(NAME sdl3-graphics COMMAND graphics-tests $<TARGET_FILE:ls-graphics>
            "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/examples/graphics/checker.bmp"
            "${CMAKE_CURRENT_BINARY_DIR}/graphics-tests-data")
        set_tests_properties(sdl3-graphics PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software" TIMEOUT 30)
        add_test(NAME sdl3-graphics-cli COMMAND ${CMAKE_COMMAND}
            "-DPARSER=$<TARGET_FILE:ls-c>" "-DMODULE=$<TARGET_FILE:ls-graphics>"
            "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}" "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/graphics-cli"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/graphics-cli.cmake)
        set_tests_properties(sdl3-graphics-cli PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")
    endif()
    if(WITH_NATIVE_MODULES)
        add_library(test-module SHARED tests/module.cpp)
        mxparser_warnings(test-module)
        add_executable(module-tests tests/modules.cpp)
        target_link_libraries(module-tests PRIVATE mxparser MXLex::mxlex)
        mxparser_warnings(module-tests)
        add_test(NAME native-modules COMMAND module-tests $<TARGET_FILE:test-module> "${CMAKE_CURRENT_BINARY_DIR}/module-tests-data")
        add_test(NAME native-modules-cli COMMAND ${CMAKE_COMMAND}
            "-DPARSER=$<TARGET_FILE:ls-c>"
            "-DMODULE=$<TARGET_FILE:test-module>"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/module-cli"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/modules-cli.cmake)
    endif()
    add_executable(native-tests tests/native.cpp)
    target_link_libraries(native-tests PRIVATE mxparser MXLex::mxlex)
    mxparser_warnings(native-tests)
    add_test(NAME native-callbacks COMMAND native-tests)
    add_executable(arithmetic-tests tests/arithmetic.cpp)
    target_include_directories(arithmetic-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    mxparser_warnings(arithmetic-tests)
    add_test(NAME portable-arithmetic COMMAND arithmetic-tests)
    add_executable(repl-tests tests/repl.cpp)
    target_link_libraries(repl-tests PRIVATE mxparser MXLex::mxlex)
    mxparser_warnings(repl-tests)
    add_test(NAME repl-state COMMAND repl-tests)
    if(UNIX)
        find_program(MXPARSER_PYTHON3 NAMES python3)
        if(MXPARSER_PYTHON3)
            add_test(NAME repl-terminal COMMAND ${MXPARSER_PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/tests/repl-terminal.py $<TARGET_FILE:ls-c>)
            set_tests_properties(repl-terminal PROPERTIES TIMEOUT 30)
        endif()
    endif()
    add_test(NAME cli-arguments COMMAND ${CMAKE_COMMAND}
        "-DPARSER=$<TARGET_FILE:ls-c>"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli-arguments"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/arguments.cmake)
    add_test(NAME repl-cli COMMAND ${CMAKE_COMMAND}
        "-DPARSER=$<TARGET_FILE:ls-c>"
        "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/repl-cli"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/repl-cli.cmake)
    add_executable(expression-tests tests/expressions.cpp)
    target_link_libraries(expression-tests PRIVATE mxparser MXLex::mxlex)
    mxparser_warnings(expression-tests)
    add_test(NAME expressions COMMAND expression-tests)
    add_executable(function-tests tests/functions.cpp)
    target_link_libraries(function-tests PRIVATE mxparser MXLex::mxlex)
    mxparser_warnings(function-tests)
    add_test(NAME functions COMMAND function-tests)
    add_executable(type-source-tests tests/types.cpp)
    target_link_libraries(type-source-tests PRIVATE mxparser MXLex::mxlex)
    mxparser_warnings(type-source-tests)
    add_test(NAME types-and-source COMMAND type-source-tests)
    add_executable(declarator-tests tests/declarators.cpp)
    target_link_libraries(declarator-tests PRIVATE mxparser MXLex::mxlex)
    mxparser_warnings(declarator-tests)
    add_test(NAME declarators COMMAND declarator-tests)
    add_executable(semantic-tests tests/semantic.cpp)
    target_link_libraries(semantic-tests PRIVATE mxparser MXLex::mxlex)
    mxparser_warnings(semantic-tests)
    add_test(NAME semantics COMMAND semantic-tests)
    add_executable(statement-tests tests/statements.cpp)
    target_link_libraries(statement-tests PRIVATE mxparser MXLex::mxlex)
    mxparser_warnings(statement-tests)
    add_test(NAME statements COMMAND statement-tests)
    add_executable(aggregate-tests tests/aggregates.cpp)
    target_link_libraries(aggregate-tests PRIVATE mxparser MXLex::mxlex)
    mxparser_warnings(aggregate-tests)
    add_test(NAME aggregates COMMAND aggregate-tests)
    add_test(NAME aggregate-syntax COMMAND ls-c --syntax-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/aggregates.c)
    add_test(NAME aggregate-check COMMAND ls-c --check-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/aggregates.c)
    add_executable(lsprintf-tests tests/lsprintf.cpp)
    target_link_libraries(lsprintf-tests PRIVATE mxparser MXLex::mxlex)
    mxparser_warnings(lsprintf-tests)
    add_test(NAME lsprintf COMMAND lsprintf-tests)
    add_test(NAME lsprintf-example COMMAND ls-c --run-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/lsprintf.c)
    set_tests_properties(lsprintf-example PROPERTIES
        PASS_REGULAR_EXPRESSION "Hello from MXParser! count=3, average=2.50"
        FAIL_REGULAR_EXPRESSION "Result:")
    add_executable(execution-tests tests/execution.cpp)
    target_link_libraries(execution-tests PRIVATE mxparser MXLex::mxlex)
    mxparser_warnings(execution-tests)
    add_test(NAME execution COMMAND execution-tests ${CMAKE_CURRENT_SOURCE_DIR}/examples/subset.c)
    add_test(NAME subset-example-run COMMAND ls-c --run-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/subset.c)
    set_tests_properties(subset-example-run PROPERTIES FAIL_REGULAR_EXPRESSION "Result:")
    add_test(NAME subset-example-result COMMAND ls-c --print-result ${CMAKE_CURRENT_SOURCE_DIR}/examples/subset.c)
    set_tests_properties(subset-example-result PROPERTIES PASS_REGULAR_EXPRESSION "Result: 2")
    add_executable(publication-tests tests/publication.cpp)
    target_link_libraries(publication-tests PRIVATE mxparser MXLex::mxlex)
    mxparser_warnings(publication-tests)
    add_test(NAME subset-publication COMMAND publication-tests ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME subset-example-syntax COMMAND ls-c --syntax-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/subset.c)
    add_test(NAME subset-example-check COMMAND ls-c --check-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/subset.c)
    find_program(MXPARSER_C99_COMPILER NAMES cc gcc clang DOC "C compiler for C99 subset syntax comparisons")
    if(MXPARSER_C99_COMPILER)
        if(NOT WIN32)
            add_test(NAME execution-c99-comparison COMMAND ${CMAKE_COMMAND}
                "-DCOMPILER=${MXPARSER_C99_COMPILER}"
                "-DPARSER=$<TARGET_FILE:ls-c>"
                "-DTESTER=$<TARGET_FILE:execution-tests>"
                "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
                "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/runtime-comparison"
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/runtime-compiler.cmake)
        endif()
        add_test(NAME subset-c99-comparison COMMAND ${CMAKE_COMMAND}
            "-DCOMPILER=${MXPARSER_C99_COMPILER}"
            "-DPARSER=$<TARGET_FILE:ls-c>"
            "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/c99-comparison"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/compiler.cmake)
    else()
        message(STATUS "C compiler unavailable: subset-c99-comparison will not be registered")
    endif()
    add_executable(subset-tests tests/subset.cpp)
    target_link_libraries(subset-tests PRIVATE mxparser MXLex::mxlex)
    mxparser_warnings(subset-tests)
    add_test(NAME subset-fixtures COMMAND subset-tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/subset/manifest.tsv)
    add_test(NAME function-example COMMAND ls-c ${CMAKE_CURRENT_SOURCE_DIR}/examples/functions.c)
    set_tests_properties(function-example PROPERTIES
        PASS_REGULAR_EXPRESSION "ICode:"
        FAIL_REGULAR_EXPRESSION "Result:")
    add_test(NAME function-syntax COMMAND ls-c --syntax-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/functions.c)
    add_test(NAME declaration-syntax COMMAND ls-c --syntax-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/declarations.c)
    add_test(NAME function-check COMMAND ls-c --check-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/functions.c)
    add_test(NAME declaration-check COMMAND ls-c --check-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/declarations.c)
    add_test(NAME statement-syntax COMMAND ls-c --syntax-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/statements.c)
    add_test(NAME statement-check COMMAND ls-c --check-program ${CMAKE_CURRENT_SOURCE_DIR}/examples/statements.c)
endif()
