cmake_minimum_required(VERSION 3.28)

project(
    sudoku
    VERSION 1.0.0
    DESCRIPTION "Offline Sudoku Game"
    LANGUAGES CXX
)

# C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sanitizer configuration (ASan + UBSan)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(SUDOKU_ENABLE_UI_TESTS "Build Qt6 UI tests (requires Qt6::Test)" OFF)

# Disable C++23 modules for compatibility with clang-tidy
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Compiler cache (ccache) for faster recompilation
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message(STATUS "ccache found: ${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
else()
    message(STATUS "ccache not found - compilation caching disabled")
endif()

# Static analysis and formatting tools
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
find_program(CLANG_FORMAT_EXE NAMES "clang-format")

if(CLANG_TIDY_EXE)
    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
    # Don't set CMAKE_CXX_CLANG_TIDY globally - we'll set it per-target
else()
    message(WARNING "clang-tidy not found - static analysis disabled")
endif()

if(CLANG_FORMAT_EXE)
    message(STATUS "clang-format found: ${CLANG_FORMAT_EXE}")
else()
    message(WARNING "clang-format not found - code formatting disabled")
endif()

# Generate compile_commands.json for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler-specific settings
if(MSVC)
    # MSVC 2024 settings
    add_compile_options(/W4 /WX /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    # GCC 15.1+ settings
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O3 -DNDEBUG)
    endif()

    # Sanitizer instrumentation (ASan + UBSan)
    if(ENABLE_SANITIZERS)
        message(STATUS "Sanitizers enabled: AddressSanitizer + UndefinedBehaviorSanitizer")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all -g)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# SIMD Configuration: Runtime CPU dispatch
# Build a single binary that auto-selects Scalar/AVX2/AVX512 at runtime.
# SIMD source files are compiled in a separate STATIC library with per-target flags.
# All other code compiles at baseline x86-64 (no AVX flags).
message(STATUS "SIMD: Runtime CPU dispatch ENABLED (Scalar/AVX2/AVX-512 auto-detection)")

# Fallback find modules (used when Conan is not available, e.g. Flatpak builds)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Dependencies (found via Conan toolchain or system)
find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(ZLIB REQUIRED)
find_package(libsodium REQUIRED)

# Qt6 integration (system-installed)
find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
qt_standard_project_setup()

# Testing framework (optional)
find_package(Catch2 QUIET)

# Project structure
set(SOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Include directories
include_directories(${INCLUDE_DIR})

# SIMD STATIC library: AVX2-specific code compiled with per-target flags
add_library(simd_avx2 STATIC "${SOURCES_DIR}/core/simd_constraint_state.cpp")
target_include_directories(simd_avx2 PUBLIC ${INCLUDE_DIR} ${SOURCES_DIR})
if(MSVC)
    target_compile_options(simd_avx2 PRIVATE /arch:AVX2)
else()
    target_compile_options(simd_avx2 PRIVATE -mavx2 -mfma)
endif()

# SIMD STATIC library: AVX-512 code compiled with per-target flags
# Uses AVX-512 BITALG for native _mm512_popcnt_epi16 (Zen 5+)
add_library(simd_avx512 STATIC "${SOURCES_DIR}/core/simd_avx512_ops.cpp")
target_include_directories(simd_avx512 PUBLIC ${INCLUDE_DIR} ${SOURCES_DIR})
if(NOT MSVC)
    target_compile_options(simd_avx512 PRIVATE -mavx512f -mavx512bw -mavx512bitalg -mavx2 -mfma)
endif()

# Source files - separate project code from third-party code
file(GLOB_RECURSE PROJECT_SOURCES
    "${SOURCES_DIR}/core/*.cpp"
    "${SOURCES_DIR}/core/*.h"
    "${SOURCES_DIR}/model/*.cpp"
    "${SOURCES_DIR}/model/*.h"
    "${SOURCES_DIR}/view/*.cpp"
    "${SOURCES_DIR}/view/*.h"
    "${SOURCES_DIR}/view_model/*.cpp"
    "${SOURCES_DIR}/view_model/*.h"
    "${SOURCES_DIR}/infrastructure/*.cpp"
    "${SOURCES_DIR}/infrastructure/*.h"
    "${SOURCES_DIR}/*.cpp"
    "${SOURCES_DIR}/*.h"
)

# Exclude SIMD source from main GLOB (compiled separately in simd_avx2/simd_avx512 libraries)
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*simd_constraint_state\\.cpp$")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*simd_avx512_ops\\.cpp$")

# All sources are project sources (no third-party backends needed with Qt6)
set(SOURCES ${PROJECT_SOURCES})

# Main executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Enable clang-tidy for project sources
if(CLANG_TIDY_EXE)
    set_source_files_properties(${PROJECT_SOURCES} PROPERTIES
        CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
    )
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME} PUBLIC
    simd_avx2
    simd_avx512
    Qt6::Widgets
    spdlog::spdlog
    yaml-cpp::yaml-cpp
    ZLIB::ZLIB
    libsodium::libsodium
)

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
elseif(UNIX AND NOT APPLE)
    # Linux-specific settings
    target_link_libraries(${PROJECT_NAME} PUBLIC pthread)
endif()

# Output directories - flattened structure
# CMAKE_BINARY_DIR is already build/Debug or build/Release from Conan layout
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Copy locale files to build output directory (next to executable)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/locales"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/locales"
    COMMENT "Copying locale files to build directory"
)

# Testing
enable_testing()
if(Catch2_FOUND)
    add_subdirectory(tests)
endif()


# ── Install rules (platform-specific)
if(WIN32)
    # Windows: flat layout for NSIS installer
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ".")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources/locales" DESTINATION ".")

    # Qt runtime via windeployqt
    # QT6_DIR is set as an environment variable by the build scripts; read it with $ENV{}
    if(NOT "$ENV{QT6_DIR}" STREQUAL "")
        set(_qt6_dir "$ENV{QT6_DIR}")
        file(TO_CMAKE_PATH "${_qt6_dir}" _qt6_dir)
        install(CODE "
            message(STATUS \"Running windeployqt on staged install...\")
            execute_process(
                COMMAND \"${_qt6_dir}/bin/windeployqt.exe\"
                        --release --no-translations
                        \"\${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.exe\"
                RESULT_VARIABLE _wdqt_result
            )
            if(NOT _wdqt_result EQUAL 0)
                message(FATAL_ERROR \"windeployqt failed: \${_wdqt_result}\")
            endif()
        ")
    endif()
else()
    # Linux: FHS-compliant layout
    include(GNUInstallDirs)
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources/locales"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/sudoku")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/linux/org.sudoku_cpp.Sudoku.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/linux/org.sudoku_cpp.Sudoku.metainfo.xml"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/org.sudoku_cpp.Sudoku.svg"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
endif()

# ── CPack configuration
set(CPACK_PACKAGE_NAME                "Sudoku")
set(CPACK_PACKAGE_VENDOR              "sudoku-cpp")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Sudoku puzzle game")
set(CPACK_PACKAGE_VERSION             "${PROJECT_VERSION}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY   "Sudoku")
set(CPACK_RESOURCE_FILE_LICENSE       "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

if(WIN32)
    # NSIS-specific settings (Windows only)
    set(CPACK_NSIS_DISPLAY_NAME           "Sudoku")
    set(CPACK_NSIS_PACKAGE_NAME           "Sudoku ${PROJECT_VERSION}")
    set(CPACK_NSIS_INSTALL_ROOT           "$LOCALAPPDATA")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY  ".")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortcut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Sudoku.lnk' '$INSTDIR\\\\${PROJECT_NAME}.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Sudoku.lnk'")
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN     "${PROJECT_NAME}.exe")

    # Output filename: Sudoku-1.0.0-win64.exe
    set(CPACK_SYSTEM_NAME                 "win64")
    set(CPACK_PACKAGE_FILE_NAME           "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}")
endif()

include(CPack)

# Static analysis targets
if(CLANG_TIDY_EXE)
    # Create a separate target for clang-tidy analysis
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXE}
            -p=${CMAKE_BINARY_DIR}
            --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
            ${SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy static analysis"
        VERBATIM
    )
    
    # Create a target to run clang-tidy with fixes
    add_custom_target(tidy-fix
        COMMAND ${CLANG_TIDY_EXE}
            -p=${CMAKE_BINARY_DIR}
            --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
            --fix
            --fix-errors
            ${SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy with automatic fixes"
        VERBATIM
    )
    
    # Create a target for HTML report generation (requires clang-tidy-html)
    find_program(CLANG_TIDY_HTML_EXE NAMES "clang-tidy-html")
    if(CLANG_TIDY_HTML_EXE)
        add_custom_target(tidy-html
            COMMAND ${CLANG_TIDY_EXE}
                -p=${CMAKE_BINARY_DIR}
                --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
                --export-fixes=clang-tidy-fixes.yaml
                ${SOURCES} || true
            COMMAND ${CLANG_TIDY_HTML_EXE} clang-tidy-fixes.yaml
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating clang-tidy HTML report"
            VERBATIM
        )
    endif()
endif()

# Code formatting targets (using script for better control)
if(CLANG_FORMAT_EXE)
    # Format all source files
    add_custom_target(format
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/format.sh
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Formatting source code with clang-format"
        VERBATIM
    )

    # Check formatting without modifying files
    add_custom_target(format-check
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/format.sh check
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking code formatting"
        VERBATIM
    )
endif()