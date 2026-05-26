cmake_minimum_required(VERSION 3.28)

# Only build tests if Catch2 is available
if(NOT Catch2_FOUND)
    message(STATUS "Catch2 not found, skipping tests")
    return()
endif()

# Include source directories for testing
include_directories(${CMAKE_SOURCE_DIR}/src)

# Collect all source files except main.cpp
file(GLOB_RECURSE LIB_SOURCES 
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.h"
)

# Remove main.cpp files from library sources
list(FILTER LIB_SOURCES EXCLUDE REGEX ".*main\\.cpp$")
list(FILTER LIB_SOURCES EXCLUDE REGEX ".*main_new\\.cpp$")

# Exclude SIMD source (compiled separately in simd_avx2/simd_avx512 libraries with per-target flags)
list(FILTER LIB_SOURCES EXCLUDE REGEX ".*simd_constraint_state\\.cpp$")
list(FILTER LIB_SOURCES EXCLUDE REGEX ".*simd_avx512_ops\\.cpp$")
# Scalar stub excluded from GLOB; added back conditionally below
list(FILTER LIB_SOURCES EXCLUDE REGEX ".*simd_constraint_state_scalar\\.cpp$")

# Remove view files that depend on Qt6 (not needed for unit tests)
list(FILTER LIB_SOURCES EXCLUDE REGEX ".*/view/.*\\.(cpp|h)$")
list(FILTER LIB_SOURCES EXCLUDE REGEX ".*log_manager\\.cpp$")

# On non-x86 platforms the AVX2/AVX-512 libs are absent; add scalar stubs instead
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686")
    list(APPEND LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/core/simd_constraint_state_scalar.cpp")
endif()

# Create a library from source files for testing (links Qt6::Core for translations)
add_library(sudoku_lib STATIC ${LIB_SOURCES})
set_target_properties(sudoku_lib PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

# Add include directories to library
target_include_directories(sudoku_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)

# Link dependencies to library
target_link_libraries(sudoku_lib PUBLIC
    spdlog::spdlog
    yaml-cpp::yaml-cpp
    ZLIB::ZLIB
    libsodium::libsodium
    Qt6::Core
)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686")
    target_link_libraries(sudoku_lib PUBLIC simd_avx2 simd_avx512)
endif()

# Platform-specific library settings
if(WIN32)
    # Windows-specific settings for library
elseif(APPLE)
    # macOS: pthreads included in libc
elseif(UNIX AND NOT APPLE)
    # Linux-specific settings for library
    target_link_libraries(sudoku_lib PUBLIC pthread)
endif()

# Test helpers library (shared utilities for all tests)
add_library(test_helpers STATIC
    helpers/test_utils.cpp
    helpers/memory_monitor.cpp
    helpers/benchmark_utils.cpp
    helpers/fixture_serializer.cpp
)

target_include_directories(test_helpers PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
)

target_link_libraries(test_helpers PUBLIC
    sudoku_lib
    yaml-cpp::yaml-cpp
)

# Unit tests
add_subdirectory(unit)

# Integration tests
add_subdirectory(integration)

# Benchmarks
add_subdirectory(benchmarks)

# Qt6 UI tests (optional, requires -DSUDOKU_ENABLE_UI_TESTS=ON)
if(SUDOKU_ENABLE_UI_TESTS)
    file(GLOB VIEW_SOURCES
        "${CMAKE_SOURCE_DIR}/src/view/*.cpp"
        "${CMAKE_SOURCE_DIR}/src/view/*.h"
    )

    add_library(sudoku_view_lib STATIC ${VIEW_SOURCES})
    set_target_properties(sudoku_view_lib PROPERTIES AUTOMOC ON)
    target_include_directories(sudoku_view_lib PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(sudoku_view_lib PUBLIC
        sudoku_lib
        Qt6::Widgets
    )
    target_compile_definitions(sudoku_view_lib PUBLIC SUDOKU_UI_TESTING)

    add_subdirectory(ui)
endif()

# Profiling executable (for callgrind/valgrind)
add_executable(profile_puzzle_generation
    profile_puzzle_generation.cpp
)

target_link_libraries(profile_puzzle_generation PRIVATE
    sudoku_lib
)

set_target_properties(profile_puzzle_generation PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Statistical profiling executable (50 Hard puzzles)
add_executable(profile_hard_puzzles
    profile_hard_puzzles.cpp
)

target_link_libraries(profile_hard_puzzles PRIVATE
    sudoku_lib
)

set_target_properties(profile_hard_puzzles PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Statistical profiling executable (50 Expert puzzles, 2s timeout)
add_executable(profile_expert_puzzles
    profile_expert_puzzles.cpp
)

target_link_libraries(profile_expert_puzzles PRIVATE
    sudoku_lib
)

set_target_properties(profile_expert_puzzles PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Test runner that runs all tests
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS unit_tests integration_tests
    COMMENT "Running all tests"
)

# Coverage target (if coverage tools available)
find_program(GCOV_PATH gcov)
find_program(LCOV_PATH lcov)
find_program(GENHTML_PATH genhtml)
find_program(GCOVR_PATH gcovr)

if((CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo") AND NOT ENABLE_SANITIZERS)
    # Add coverage flags to the library
    target_compile_options(sudoku_lib PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_libraries(sudoku_lib PRIVATE --coverage)

    # Add coverage flags to test executables (required for header-only code coverage)
    # gcovr filter (src/.*) ensures only source files appear in reports
    target_compile_options(unit_tests PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_libraries(unit_tests PRIVATE --coverage)
    target_compile_options(integration_tests PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_libraries(integration_tests PRIVATE --coverage)
    target_compile_options(test_helpers PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    target_link_libraries(test_helpers PRIVATE --coverage)

    # gcovr coverage targets (preferred - more modern and flexible)
    if(GCOVR_PATH)
        # Text summary coverage report
        add_custom_target(coverage
            COMMAND ${GCOVR_PATH} -r ${CMAKE_SOURCE_DIR} 
                --exclude='${CMAKE_SOURCE_DIR}/tests/.*'
                --exclude='${CMAKE_SOURCE_DIR}/build/.*'
                --exclude='${CMAKE_SOURCE_DIR}/.*conan.*'
                --exclude='/usr/.*'

                --print-summary
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS unit_tests integration_tests
            COMMENT "Generating gcovr coverage summary"
        )
        
        # HTML coverage report
        add_custom_target(coverage-html
            COMMAND ${GCOVR_PATH} -r ${CMAKE_SOURCE_DIR}
                --exclude='${CMAKE_SOURCE_DIR}/tests/.*'
                --exclude='${CMAKE_SOURCE_DIR}/build/.*'
                --exclude='${CMAKE_SOURCE_DIR}/.*conan.*'
                --exclude='/usr/.*'

                --html --html-details
                --output coverage_report.html
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS unit_tests integration_tests
            COMMENT "Generating gcovr HTML coverage report"
        )
        
        # XML coverage report (for CI/CD integration)
        add_custom_target(coverage-xml
            COMMAND ${GCOVR_PATH} -r ${CMAKE_SOURCE_DIR}
                --exclude='${CMAKE_SOURCE_DIR}/tests/.*'
                --exclude='${CMAKE_SOURCE_DIR}/build/.*'
                --exclude='${CMAKE_SOURCE_DIR}/.*conan.*'
                --exclude='/usr/.*'

                --xml --output coverage.xml
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS unit_tests integration_tests
            COMMENT "Generating gcovr XML coverage report"
        )
        
        # JSON coverage report (for programmatic analysis)
        add_custom_target(coverage-json
            COMMAND ${GCOVR_PATH} -r ${CMAKE_SOURCE_DIR}
                --exclude='${CMAKE_SOURCE_DIR}/tests/.*'
                --exclude='${CMAKE_SOURCE_DIR}/build/.*'
                --exclude='${CMAKE_SOURCE_DIR}/.*conan.*'
                --exclude='/usr/.*'

                --json --output coverage.json
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS unit_tests integration_tests
            COMMENT "Generating gcovr JSON coverage report"
        )
        
        # Comprehensive coverage target that generates all formats
        add_custom_target(coverage-all
            DEPENDS coverage coverage-html coverage-xml coverage-json
            COMMENT "Generating all coverage report formats"
        )
    endif()
    
    # lcov coverage targets (fallback - traditional approach)
    if(GCOV_PATH AND LCOV_PATH AND GENHTML_PATH)
        add_custom_target(coverage-lcov
            COMMAND ${LCOV_PATH} --directory . --capture --output-file coverage.info
            COMMAND ${LCOV_PATH} --remove coverage.info '/usr/*' '*/tests/*' '*/build/*' '*/.*conan.*' --output-file coverage.info
            COMMAND ${LCOV_PATH} --list coverage.info
            COMMAND ${GENHTML_PATH} coverage.info --output-directory coverage_html_lcov
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS unit_tests integration_tests
            COMMENT "Generating lcov coverage report"
        )
    endif()
endif()