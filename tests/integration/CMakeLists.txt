cmake_minimum_required(VERSION 3.28)

# Collect all integration test files.
# NOTE: plain GLOB (not CONFIGURE_DEPENDS) — adding/removing a *.cpp here requires a reconfigure
# (`cmake build/<preset>` or touch this file) so incremental builds re-glob; CI cold-configures.
# Last touched when test_class_c_bucket_migration_integration.cpp was added (story 0b.4c).
file(GLOB_RECURSE INTEGRATION_TEST_SOURCES "*.cpp")

# Custom Catch2 entry point that owns a QCoreApplication for the run.
# See tests/helpers/qt_test_main.cpp for rationale. Replaces Catch2WithMain.
add_executable(integration_tests
    ${INTEGRATION_TEST_SOURCES}
    ${CMAKE_SOURCE_DIR}/tests/helpers/qt_test_main.cpp
)

target_link_libraries(integration_tests PUBLIC
    sudoku_lib
    test_helpers
    Catch2::Catch2
)

# MSVC: suppress C4834 (discarded [[nodiscard]] result) and C4702 (unreachable code in templates) in tests
if(MSVC)
    target_compile_options(integration_tests PRIVATE /wd4834 /wd4702)
endif()

# Add to test suite
add_test(NAME integration_tests COMMAND integration_tests)

# Set output directory
set_target_properties(integration_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
)