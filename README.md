# Sudoku C++

[![CI](https://github.com/darkstar79/sudoku-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/darkstar79/sudoku-cpp/actions/workflows/ci.yml)
[![Nightly](https://github.com/darkstar79/sudoku-cpp/actions/workflows/nightly.yml/badge.svg)](https://github.com/darkstar79/sudoku-cpp/actions/workflows/nightly.yml)

An offline Sudoku game built with C++23 and Qt6.

This project is entirely **vibe coded** using [Claude Code](https://docs.anthropic.com/en/docs/claude-code) — no manual coding involved. It serves as a personal experiment to explore what's possible with AI-assisted development and how to work effectively with Claude Code on a non-trivial C++ codebase.

## Features

- Puzzle generation with 5 difficulty levels and guaranteed unique solutions
- 42 solving strategies with step-by-step hints
- Undo/redo, pencil marks, keyboard navigation
- Encrypted save/load (YAML + zlib + libsodium)
- Statistics tracking
- SIMD-accelerated solver (AVX2/AVX-512)

## Technology Stack

| | |
|---|---|
| **Language** | C++23 |
| **UI** | Qt6 Widgets |
| **Build** | CMake + Conan |
| **Testing** | Catch2 |
| **Architecture** | MVVM |

## Prerequisites

- C++23 compatible compiler (GCC 13+, Clang 16+, or MSVC 2022+)
- CMake 3.25+
- Conan 2.0+
- Python 3.8+ (for Conan)
- **ccache** (optional, recommended for faster recompilation)

### Installing Conan (if not already installed)

```bash
pip install conan
```

### Installing ccache (optional, for faster recompilation)

ccache caches compilation results to speed up rebuilds:

```bash
# Fedora/RHEL
sudo dnf install ccache

# Ubuntu/Debian
sudo apt install ccache

# macOS
brew install ccache
```

The build system will automatically detect and use ccache if installed.

### Conan Profiles

The project includes compiler-versioned Conan profiles for reproducible builds:

- **GCC 15:** `gcc-15-release`, `gcc-15-debug`, `gcc-15-relwithdebinfo`
- **Clang 21:** `clang-21-release`, `clang-21-debug`, `clang-21-relwithdebinfo`

See [CONAN_PROFILES.md](docs/CONAN_PROFILES.md) for detailed profile documentation and usage examples.

## Building the Project

### Windows (MSVC + Qt6)

**Prerequisites:**

1. **Visual Studio 2022/2026** with the *Desktop development with C++* workload
2. **Qt6** — install via [Qt Online Installer](https://www.qt.io/download-qt-installer), selecting the **MSVC 2022 64-bit** component
3. **Conan 2** — `pip install conan`
4. **CMake** — bundled with Conan or install separately

**One-time setup:**

```bat
REM Generate a Conan profile matching your MSVC installation
conan profile detect --name msvc-release
conan profile detect --name msvc-debug

REM Point CMake to your Qt6 MSVC kit (adjust version path)
set QT6_DIR=C:\Qt\6.10.0\msvc2022_64
```

**Build and run:**

```bat
REM Release build
scripts\build_windows.bat

REM Debug build
scripts\build_windows_debug.bat

REM Run
build\Release\bin\sudoku.exe
```

The build scripts auto-detect the Visual Studio installation via `vswhere`. The `QT6_DIR` variable can also be set permanently in your system environment variables to avoid setting it each session.

See [CONAN_PROFILES.md](docs/CONAN_PROFILES.md) for MSVC profile details and manual profile configuration.

**Creating a Windows installer:**

Requires [NSIS](https://nsis.sourceforge.io/Download) (`winget install NSIS.NSIS`).

```bat
scripts\create_installer.bat
```

Produces `Sudoku-1.0.0-win64.exe` — a standard installer with Start Menu shortcut and Add/Remove Programs entry.

---

### Quick Start (Linux/macOS)

```bash
# Install dependencies and configure
conan install . --build=missing
cmake --preset conan-release

# Build
cmake --build --preset conan-release

# Run
./build/Release/bin/sudoku
```

### Build Configurations

The project supports three build configurations with compiler-specific profiles:

#### 1. Release Build (Recommended for normal use)

Optimized build without debug symbols:

```bash
# GCC (default)
conan install . --build=missing
cmake --preset conan-release
cmake --build --preset conan-release
./build/Release/bin/sudoku

# Clang (alternative)
conan install . --profile=clang-21-release --build=missing --output-folder=build/Clang-Release
cmake -S . -B build/Clang-Release -DCMAKE_TOOLCHAIN_FILE=build/Clang-Release/conan_toolchain.cmake
cmake --build build/Clang-Release
./build/Clang-Release/bin/sudoku
```

#### 2. Debug Build

Unoptimized build with full debug symbols for development:

```bash
# GCC (recommended for debugging)
conan install . --profile=gcc-15-debug --build=missing --output-folder=build/Debug
cmake --preset conan-debug
cmake --build --preset conan-debug
./build/Debug/bin/sudoku

# Clang (alternative)
conan install . --profile=clang-21-debug --build=missing --output-folder=build/Clang-Debug
cmake -S . -B build/Clang-Debug -DCMAKE_TOOLCHAIN_FILE=build/Clang-Debug/conan_toolchain.cmake
cmake --build build/Clang-Debug
./build/Clang-Debug/bin/sudoku
```

Debug builds include:
- Full debug symbols in **all dependencies** (yaml-cpp, spdlog, etc.)
- Runtime bounds checking and assertions (`-D_GLIBCXX_ASSERTIONS`)
- No optimization (`-O0`)
- Complete stack traces (`-fno-omit-frame-pointer`)

#### 3. RelWithDebInfo Build

Optimized build with debug symbols for profiling:

```bash
# GCC
conan install . --profile=gcc-15-relwithdebinfo --build=missing --output-folder=build/RelWithDebInfo
cmake -S . -B build/RelWithDebInfo -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_TOOLCHAIN_FILE=build/RelWithDebInfo/conan_toolchain.cmake
cmake --build build/RelWithDebInfo
./build/RelWithDebInfo/bin/sudoku

# Clang
conan install . --profile=clang-21-relwithdebinfo --build=missing --output-folder=build/Clang-RelWithDebInfo
cmake -S . -B build/Clang-RelWithDebInfo -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_TOOLCHAIN_FILE=build/Clang-RelWithDebInfo/conan_toolchain.cmake
cmake --build build/Clang-RelWithDebInfo
```

## Testing

### Running Tests

```bash
# Run all tests (unit + integration)
./build/Release/bin/tests/unit_tests && ./build/Release/bin/tests/integration_tests

# Run specific test case by name
./build/Release/bin/tests/unit_tests "GameValidator - Move Validation"

# Run tests with specific tag
./build/Release/bin/tests/unit_tests "[game_validator]"

# Verbose output for debugging
./build/Release/bin/tests/unit_tests --success
```

### Code Coverage

Generate test coverage reports:

```bash
# Quick coverage summary
./scripts/coverage.sh

# HTML detailed report
./scripts/coverage.sh html
firefox coverage_html/index.html

# All report formats
./scripts/coverage.sh all

# Clean rebuild with coverage
./scripts/coverage.sh clean
```

### Code Quality Tools

```bash
# Format code (clang-format)
./scripts/format.sh

# Check formatting without changes
./scripts/format.sh check

# Static analysis (clang-tidy)
./scripts/tidy.sh check

# Apply automatic fixes
./scripts/tidy.sh fix
```

## Project Structure

```
sudoku-cpp/
├── src/
│   ├── core/              # Business logic (Model layer)
│   ├── model/             # Domain models
│   ├── view_model/        # Presentation logic
│   ├── view/              # UI rendering (Qt6 Widgets)
│   └── infrastructure/    # Cross-cutting concerns
├── include/               # Shared headers
├── tests/
│   ├── unit/              # Unit tests
│   ├── integration/       # Integration tests
│   └── helpers/           # Test utilities
├── scripts/               # Build and analysis scripts
├── CMakeLists.txt         # Build configuration
└── conanfile.py          # Dependency management
```

## Architecture

The project follows MVVM (Model-View-ViewModel) architecture with strict separation of concerns:

- **Model Layer:** Business logic (GameValidator, PuzzleGenerator, SudokuSolver, SaveManager)
- **ViewModel Layer:** Presentation logic with Observable pattern
- **View Layer:** UI rendering with Qt6 Widgets

Key architectural principles:
- Dependency Injection via interfaces
- Single-threaded design (no mutexes needed)
- Observable pattern for reactive UI updates
- Test-Driven Development (TDD)

## Performance

The puzzle generator achieves high performance through Zobrist hashing, memoization, SIMD constraint propagation, and runtime CPU dispatch (Scalar/AVX2/AVX-512):

- **Easy:** < 1ms (instant)
- **Medium:** ~9ms
- **Hard:** ~64ms median
- **Expert:** < 50ms
- **Master:** < 50ms

Five optimization phases delivered 80-4000x speedup over naive backtracking. See [PERFORMANCE_MEASUREMENTS.md](docs/PERFORMANCE_MEASUREMENTS.md) for detailed benchmarks.

## Development

### Pre-Commit Hook

Install the pre-commit hook for automatic checks on every commit:

```bash
./scripts/setup-hooks.sh
```

The hook checks GPLv3 license headers and code formatting on staged files (~instant). Optionally run clang-tidy on changed lines with `TIDY=1 git commit`.

### Manual Quality Checks

```bash
# All tests pass
./build/Release/bin/tests/unit_tests && ./build/Release/bin/tests/integration_tests

# Coverage targets met (≥80% line, ≥70% function)
./scripts/coverage.sh

# Static analysis
./scripts/tidy.sh check
```

### Cross-Compiler Testing

Test with both GCC and Clang to catch compiler-specific issues:

```bash
# Build with GCC
conan install . --profile=gcc-15-release --build=missing --output-folder=build/GCC-Release
cmake -S . -B build/GCC-Release -DCMAKE_TOOLCHAIN_FILE=build/GCC-Release/conan_toolchain.cmake
cmake --build build/GCC-Release

# Build with Clang
conan install . --profile=clang-21-release --build=missing --output-folder=build/Clang-Release
cmake -S . -B build/Clang-Release -DCMAKE_TOOLCHAIN_FILE=build/Clang-Release/conan_toolchain.cmake
cmake --build build/Clang-Release

# Run tests with both compilers
./build/GCC-Release/bin/tests/unit_tests
./build/Clang-Release/bin/tests/unit_tests
```

### Debugging

For debugging crashes or investigating issues, use Debug builds with full debug symbols in all dependencies:

```bash
# Build debug version with GCC (recommended)
conan install . --profile=gcc-15-debug --build=missing --output-folder=build/Debug
cmake --preset conan-debug
cmake --build --preset conan-debug

# Run with GDB
gdb --args ./build/Debug/bin/tests/unit_tests "Failing Test"
(gdb) run
(gdb) bt full
(gdb) frame 5
(gdb) print variable_name
```

**Why Debug builds matter:**
- Debug symbols in **all dependencies** (yaml-cpp, spdlog, etc.)
- Bounds checking catches bugs that Release builds miss (`std::vector::operator[]`)
- No optimization prevents variables from being optimized away
- Complete stack traces with `-fno-omit-frame-pointer`

See [DEBUGGING_GUIDE.md](docs/DEBUGGING_GUIDE.md) for detailed debugging best practices.

## License

This project is licensed under the **GNU General Public License v3.0** (GPLv3) - see the [LICENSE](LICENSE) file for details.

GPLv3 is a copyleft license that ensures this software and all derivative works remain free and open source:
- Use, study, and modify the software freely
- Distribute copies and modified versions
- Commercial use is allowed (source must be provided to recipients)
- All derivatives must also be GPLv3-licensed

See [ACKNOWLEDGMENTS.md](ACKNOWLEDGMENTS.md) for credits and [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) for dependency licenses.

## Target Users

- Users who prefer keyboard navigation
- Desktop users (Windows/Linux)
- Offline-only operation
