cmake_minimum_required(VERSION 3.28)

# macOS: set Universal Binary default and deployment target before project() so
# CMake's compiler detection honours these settings. Override for single-arch
# local builds: cmake --preset debug -DCMAKE_OSX_ARCHITECTURES=arm64
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS deployment target")
endif()

project(
    sudoku
    VERSION 1.0.0
    DESCRIPTION "Offline Sudoku Game"
    LANGUAGES CXX
)

# Generate version header from PROJECT_VERSION. CMake project() is the single
# source of truth; main.cpp and tests consume sudoku::kAppVersion.
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/sudoku/version.h"
    @ONLY
)

# C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sanitizer configuration (ASan + UBSan)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(SUDOKU_ENABLE_UI_TESTS "Build Qt6 UI tests (requires Qt6::Test)" OFF)
option(SUDOKU_ENABLE_COVERAGE "Add gcov coverage instrumentation to Debug/RelWithDebInfo builds" ON)

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

# SIMD Configuration: Runtime CPU dispatch (x86_64 only)
# Builds Scalar/AVX2/AVX-512 variants in separate STATIC libraries; the main
# binary selects the fastest path at runtime via CPUID.
# On ARM64 (Apple Silicon) the AVX libraries are omitted and the scalar path
# in constraint_state.cpp is used exclusively.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686")
    message(STATUS "SIMD: Runtime CPU dispatch ENABLED (Scalar/AVX2/AVX-512 auto-detection)")
else()
    message(STATUS "SIMD: x86 SIMD libraries skipped on ${CMAKE_SYSTEM_PROCESSOR} — scalar path used")
endif()

# Fallback find modules (used when Conan is not available, e.g. Flatpak builds)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Dependencies (found via Conan toolchain or system)
find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(ZLIB REQUIRED)
find_package(libsodium REQUIRED)

# Qt6 integration (system-installed)
find_package(Qt6 REQUIRED COMPONENTS Widgets Test LinguistTools)
# qt_standard_project_setup() was added in Qt 6.3. Distros pinned to the Qt 6.2
# LTS (e.g. Ubuntu 22.04 / arm64, which ships 6.2.4) don't have it, so fall back
# to the equivalent AUTOMOC/AUTOUIC/AUTORCC setup it performs.
if(COMMAND qt_standard_project_setup)
    qt_standard_project_setup(I18N_TRANSLATED_LANGUAGES en de)
else()
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
endif()

# Testing framework (optional)
find_package(Catch2 QUIET)

# Project structure
set(SOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

# Include directories
# SOURCES_DIR is listed explicitly so headers in src/core/ (e.g. cpu_features.h)
# are reachable on all platforms. Previously they were only transitively available
# via the SIMD static libs' PUBLIC include dirs; those libs are skipped on ARM64.
include_directories(${INCLUDE_DIR} ${SOURCES_DIR})

# SIMD STATIC libraries (x86_64 only — not compiled on ARM64/Apple Silicon)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686")
    # AVX2-specific code
    add_library(simd_avx2 STATIC "${SOURCES_DIR}/core/simd_constraint_state.cpp")
    target_include_directories(simd_avx2 PUBLIC ${INCLUDE_DIR} ${SOURCES_DIR})
    if(MSVC)
        target_compile_options(simd_avx2 PRIVATE /arch:AVX2)
    else()
        target_compile_options(simd_avx2 PRIVATE -mavx2 -mfma)
    endif()

    # AVX-512 code — uses BITALG for native _mm512_popcnt_epi16 (Zen 5+)
    add_library(simd_avx512 STATIC "${SOURCES_DIR}/core/simd_avx512_ops.cpp")
    target_include_directories(simd_avx512 PUBLIC ${INCLUDE_DIR} ${SOURCES_DIR})
    if(NOT MSVC)
        target_compile_options(simd_avx512 PRIVATE -mavx512f -mavx512bw -mavx512bitalg -mavx2 -mfma)
    endif()
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
# Scalar stub for non-x86: excluded from GLOB, added back conditionally below
list(FILTER PROJECT_SOURCES EXCLUDE REGEX ".*simd_constraint_state_scalar\\.cpp$")

# All sources are project sources (no third-party backends needed with Qt6)
set(SOURCES ${PROJECT_SOURCES})

# On non-x86 platforms (ARM64/Apple Silicon) the AVX2/AVX-512 libs are not built.
# Add scalar stubs that satisfy the linker without any SIMD intrinsics.
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686")
    list(APPEND SOURCES "${SOURCES_DIR}/core/simd_constraint_state_scalar.cpp")
endif()

# Main executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Generated version header (sudoku/version.h)
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

# Enable clang-tidy for project sources
if(CLANG_TIDY_EXE)
    set_source_files_properties(${PROJECT_SOURCES} PROPERTIES
        CXX_CLANG_TIDY "${CLANG_TIDY_EXE}"
    )
endif()

# Link libraries
target_link_libraries(${PROJECT_NAME} PUBLIC
    Qt6::Widgets
    spdlog::spdlog
    yaml-cpp::yaml-cpp
    ZLIB::ZLIB
    libsodium::libsodium
)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686")
    target_link_libraries(${PROJECT_NAME} PUBLIC simd_avx2 simd_avx512)
endif()

# Platform-specific settings
if(WIN32)
    # Windows-specific settings
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    # Embed application icon into the .exe so Explorer/taskbar/Alt-Tab show it.
    # rc.exe resolves ICON paths relative to the .rc file's directory.
    enable_language(RC)
    target_sources(${PROJECT_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/sudoku.rc"
    )
elseif(APPLE)
    # macOS: build as an app bundle
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "Sudoku"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.darkstar79.Sudoku"
        MACOSX_BUNDLE_ICON_FILE "AppIcon"
    )
    # Ship AppIcon.icns inside the bundle when it exists (generated separately
    # from the SVG via iconutil; see docs/internal/MACOS_PORTING.md).
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/AppIcon.icns")
        set_source_files_properties(
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/AppIcon.icns"
            PROPERTIES MACOSX_PACKAGE_LOCATION "Resources"
        )
        target_sources(${PROJECT_NAME} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/resources/macos/AppIcon.icns"
        )
    endif()
elseif(UNIX AND NOT APPLE)
    # Linux-specific settings
    target_link_libraries(${PROJECT_NAME} PUBLIC pthread)
endif()

# Output directories - flattened structure
# CMAKE_BINARY_DIR is already build/Debug or build/Release from Conan layout
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Qt Linguist translations: compile .ts -> .qm and copy alongside the executable.
# Refresh .ts files from source after editing tr() call sites:
#   cmake --build build/Debug --target sudoku_lupdate
set(SUDOKU_TS_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/translations/sudoku_en.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/translations/sudoku_de.ts"
)
qt_add_lupdate(${PROJECT_NAME}
    TS_FILES ${SUDOKU_TS_FILES}
    SOURCES ${PROJECT_SOURCES}
    OPTIONS
        -tr-function-alias translate+=loc
        -no-obsolete
        -locations none
)
qt_add_lrelease(${PROJECT_NAME}
    TS_FILES ${SUDOKU_TS_FILES}
    QM_FILES_OUTPUT_VARIABLE SUDOKU_QM_FILES
)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SUDOKU_QM_FILES}
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/translations/"
    COMMENT "Copying Qt translation .qm files to build directory"
)
# On macOS the executable lives inside the bundle (Contents/MacOS/); also copy
# translations to Contents/Resources/translations/ for correct bundle layout.
if(APPLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources/translations"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/translations"
            "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources/translations"
        COMMENT "Copying Qt translations into macOS bundle Resources"
    )
endif()

# Testing
enable_testing()
if(Catch2_FOUND)
    add_subdirectory(tests)
endif()


# ── Install rules (platform-specific)
if(WIN32)
    # Windows: flat layout for NSIS installer
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ".")
    install(FILES ${SUDOKU_QM_FILES} DESTINATION "translations")
    # Ship the .ico so the Add/Remove Programs entry can reference it.
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/sudoku.ico"
        DESTINATION ".")

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
elseif(APPLE)
    # macOS: install the whole app bundle, then run macdeployqt to embed Qt
    install(TARGETS ${PROJECT_NAME} BUNDLE DESTINATION .)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt
        HINTS "${Qt6_DIR}/../../../bin"
    )
    if(MACDEPLOYQT_EXECUTABLE)
        install(CODE "
            message(STATUS \"Running macdeployqt...\")
            execute_process(
                COMMAND \"${MACDEPLOYQT_EXECUTABLE}\"
                        \"\${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app\"
                RESULT_VARIABLE _mdqt_result
            )
            if(NOT _mdqt_result EQUAL 0)
                message(FATAL_ERROR \"macdeployqt failed: \${_mdqt_result}\")
            endif()
        ")
    else()
        message(WARNING "macdeployqt not found — Qt frameworks will not be bundled")
    endif()
else()
    # Linux: FHS-compliant layout
    include(GNUInstallDirs)
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES ${SUDOKU_QM_FILES}
        DESTINATION "${CMAKE_INSTALL_DATADIR}/sudoku/translations")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/linux/io.github.darkstar79.Sudoku.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/linux/io.github.darkstar79.Sudoku.metainfo.xml"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/io.github.darkstar79.Sudoku.svg"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
endif()

# ── CPack configuration
set(CPACK_PACKAGE_NAME                "Sudoku")
set(CPACK_PACKAGE_VENDOR              "sudoku")
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
    # Branding: same icon as AppImage/Flatpak (rasterized from the SVG).
    set(CPACK_NSIS_MUI_ICON               "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/sudoku.ico")
    set(CPACK_NSIS_MUI_UNIICON            "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/sudoku.ico")
    # Icon shown in Add/Remove Programs (path is relative to $INSTDIR).
    set(CPACK_NSIS_INSTALLED_ICON_NAME    "sudoku.ico")

    # Output filename: Sudoku-<version>-win64.exe
    set(CPACK_SYSTEM_NAME                 "win64")
    set(CPACK_PACKAGE_FILE_NAME           "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}")
elseif(APPLE)
    # DragNDrop .dmg (macOS)
    set(CPACK_GENERATOR               "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME         "Sudoku")
    set(CPACK_DMG_FORMAT              "UDBZ")
    set(CPACK_PACKAGE_FILE_NAME       "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-macOS")
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