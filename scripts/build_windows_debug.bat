@echo off
REM Build script for Windows with MSVC (Debug)
REM
REM Prerequisites:
REM   - Visual Studio 2022/2026 with "Desktop development with C++" workload
REM   - Qt6 installed via Qt Online Installer (msvc2022_64 kit)
REM   - Conan 2 and CMake available on PATH
REM
REM Qt6 discovery:
REM   If QT6_DIR is set, it is used as-is. Otherwise the script auto-detects
REM   the newest C:\Qt\6.*\msvc2022_64 install with a valid Qt6 CMake config.
REM   Override example:
REM     set QT6_DIR=D:\Qt\6.11.1\msvc2022_64

REM Self-logging: if not already redirected, re-invoke with output to build_log_debug.txt
if not defined BUILD_LOGGING (
    set BUILD_LOGGING=1
    cmd /c "%~f0" %* > "%~dp0build_log_debug.txt" 2>&1
    exit /b %ERRORLEVEL%
)

setlocal EnableDelayedExpansion

echo ==================================================
echo Building sudoku-cpp for Windows (Debug)
echo ==================================================

REM ---------------------------------------------------------------------------
REM 1. Locate Visual Studio via vswhere (includes Insiders/Preview releases)
REM ---------------------------------------------------------------------------
set "VS_INSTALLER=%SystemDrive%\Program Files (x86)\Microsoft Visual Studio\Installer"
if exist "%VS_INSTALLER%\vswhere.exe" goto :vswhere_ok
echo ERROR: vswhere.exe not found. Install Visual Studio Installer.
exit /b 1
:vswhere_ok
set "PATH=%VS_INSTALLER%;%PATH%"

for /f "usebackq tokens=*" %%i in (`vswhere -latest -prerelease -products * -property installationPath`) do set "VS_PATH=%%i"
if defined VS_PATH goto :vs_found
echo ERROR: No Visual Studio installation found (including previews).
exit /b 1
:vs_found
echo Found Visual Studio: %VS_PATH%

call "%VS_PATH%\VC\Auxiliary\Build\vcvarsall.bat" x64
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Failed to initialize Visual Studio environment
    exit /b %ERRORLEVEL%
)

REM ---------------------------------------------------------------------------
REM 2. Locate Conan
REM ---------------------------------------------------------------------------
where conan >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: conan not found on PATH. Install with: pip install conan
    exit /b 1
)

REM ---------------------------------------------------------------------------
REM 3. Resolve Qt6 path
REM    Auto-detects the newest C:\Qt\6.*\msvc2022_64 kit (proper version sort,
REM    so 6.11 ranks above 6.9). Set QT6_DIR to override.
REM ---------------------------------------------------------------------------
if not defined QT6_DIR (
    for /f "usebackq delims=" %%d in (`powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0find_qt6.ps1"`) do set "QT6_DIR=%%d"
)
if not defined QT6_DIR (
    echo ERROR: Qt6 not found. Install the MSVC 2022 64-bit kit via the Qt
    echo        Online Installer, or set QT6_DIR manually, e.g.:
    echo            set QT6_DIR=C:\Qt\6.11.1\msvc2022_64
    exit /b 1
)
echo Using Qt6: %QT6_DIR%

REM ---------------------------------------------------------------------------
REM 4. Install Conan dependencies (Debug)
REM ---------------------------------------------------------------------------
echo [1/3] Installing dependencies with Conan (Debug)...
conan install . --build=missing --output-folder=build\Debug -s build_type=Debug
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Conan dependency installation failed
    exit /b %ERRORLEVEL%
)

REM ---------------------------------------------------------------------------
REM 5. Configure CMake
REM ---------------------------------------------------------------------------
echo [2/3] Configuring CMake (Debug)...
call build\Debug\generators\conanbuild.bat
cmake -S . -B build\Debug -DCMAKE_TOOLCHAIN_FILE=build\Debug\generators\conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_PREFIX_PATH=%QT6_DIR%"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed
    exit /b %ERRORLEVEL%
)

REM ---------------------------------------------------------------------------
REM 6. Build
REM ---------------------------------------------------------------------------
echo [3/4] Building project (Debug)...
cmake --build build\Debug --config Debug
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed
    exit /b %ERRORLEVEL%
)

REM ---------------------------------------------------------------------------
REM 7. Deploy Qt runtime DLLs
REM ---------------------------------------------------------------------------
echo [4/4] Deploying Qt runtime (windeployqt)...
"%QT6_DIR%\bin\windeployqt.exe" --debug --no-translations build\Debug\bin\Debug\sudoku.exe
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: windeployqt failed
    exit /b %ERRORLEVEL%
)

echo ==================================================
echo Debug build successful!
echo Executable: build\Debug\bin\Debug\sudoku.exe
echo ==================================================
