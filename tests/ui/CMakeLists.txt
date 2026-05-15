cmake_minimum_required(VERSION 3.28)

set(UI_TEST_SOURCES
    test_main_window_construction.cpp
    test_board_interaction.cpp
    test_keyboard_handling.cpp
    test_menu_toolbar_actions.cpp
    test_view_model_binding.cpp
    test_training_widget.cpp
    test_language_change.cpp
    test_import_dialog.cpp
)

foreach(TEST_SOURCE ${UI_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    set_target_properties(${TEST_NAME} PROPERTIES AUTOMOC ON)
    target_link_libraries(${TEST_NAME} PRIVATE
        sudoku_view_lib
        test_helpers
        Qt6::Test
        Qt6::Widgets
    )
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
    )
    # SUDOKU_QM_DIR points at the directory where qt_add_lrelease emits
    # sudoku_<locale>.qm files. Used by test_language_change to load the
    # bundled German translator. Built by the main 'sudoku' target's
    # lrelease step.
    target_compile_definitions(${TEST_NAME} PRIVATE
        SUDOKU_QM_DIR="${CMAKE_BINARY_DIR}"
    )
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_target_properties(${TEST_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests/ui"
    )
    set_tests_properties(${TEST_NAME} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endforeach()
