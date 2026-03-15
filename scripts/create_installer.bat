@echo off
setlocal enabledelayedexpansion

REM ---------------------------------------------------------------------------
REM Self-logging: re-invoke with output redirected to installer_log.txt
REM ---------------------------------------------------------------------------
if not defined BUILD_LOGGING (
    set BUILD_LOGGING=1
    cmd.exe /c "%~f0" %* > "%~dp0installer_log.txt" 2>&1
    exit /b %ERRORLEVEL%
)

echo ==================================================
echo  Sudoku Windows Installer Builder
echo ==================================================

REM ---------------------------------------------------------------------------
REM 1. Check NSIS (also check default install location if not on PATH)
REM ---------------------------------------------------------------------------
where makensis.exe >nul 2>&1
if %ERRORLEVEL% EQU 0 goto :nsis_ok

set "NSIS_DEFAULT=%SystemDrive%\Program Files (x86)\NSIS"
if exist "%NSIS_DEFAULT%\makensis.exe" (
    set "PATH=%NSIS_DEFAULT%;%PATH%"
    goto :nsis_ok
)

echo ERROR: NSIS not found. Install with one of:
echo   winget install NSIS.NSIS
echo   choco install nsis
echo Then ensure makensis.exe is on your PATH.
exit /b 1

:nsis_ok
echo Found NSIS: OK

REM ---------------------------------------------------------------------------
REM 2. Find VS (needed to locate cmake 4.x which supports VS 18 2026 generator)
REM    ConanCenter only has cmake up to 3.31 which does not know VS 18 2026.
REM ---------------------------------------------------------------------------
set "VS_INSTALLER=%SystemDrive%\Program Files (x86)\Microsoft Visual Studio\Installer"
if exist "%VS_INSTALLER%\vswhere.exe" goto :vswhere_ok
echo ERROR: vswhere.exe not found.
exit /b 1
:vswhere_ok
set "PATH=%VS_INSTALLER%;%PATH%"

for /f "usebackq tokens=*" %%i in (`vswhere -latest -prerelease -products * -property installationPath`) do set "VS_PATH=%%i"
if not defined VS_PATH (
    echo ERROR: No Visual Studio installation found.
    exit /b 1
)

set "VS_CMAKE_BIN=%VS_PATH%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
if exist "%VS_CMAKE_BIN%\cmake.exe" goto :cmake_ok
echo ERROR: cmake not found in VS installation at %VS_CMAKE_BIN%
exit /b 1
:cmake_ok
echo Found VS cmake: %VS_CMAKE_BIN%\cmake.exe

REM ---------------------------------------------------------------------------
REM 3. Release build (incremental - skips if already up to date)
REM ---------------------------------------------------------------------------
echo [1/2] Building Release...
call "%~dp0build_windows.bat"
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Release build failed. See scripts\build_log.txt for details.
    exit /b %ERRORLEVEL%
)

REM ---------------------------------------------------------------------------
REM 4. CPack: stage install tree + run NSIS
REM    Use VS cmake 4.x (supports "Visual Studio 18 2026" generator in CPackConfig)
REM ---------------------------------------------------------------------------
echo [2/2] Packaging installer...
set "PATH=%VS_CMAKE_BIN%;%PATH%"
"%VS_CMAKE_BIN%\cmake.exe" -E chdir build\Release cpack -G NSIS -C Release
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CPack/NSIS packaging failed.
    exit /b %ERRORLEVEL%
)

REM Move installer to project root
for %%f in (build\Release\Sudoku-*-win64.exe) do move "%%f" "." >nul 2>&1

echo ==================================================
echo Installer ready!
for %%f in (Sudoku-*-win64.exe) do echo Installer: %%f
echo ==================================================
