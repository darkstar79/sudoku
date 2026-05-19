# Build script for Windows with MSVC.
#
# Usage:
#   .\scripts\build_windows.ps1                    # Release (default)
#   .\scripts\build_windows.ps1 -Config Debug      # Debug
#
# Prerequisites:
#   - Visual Studio 2022 or 2026 with "Desktop development with C++" workload
#   - Qt6 installed via Qt Online Installer (MSVC 2022 64-bit kit)
#   - Python venv activated with: uv pip install -r requirements.txt
#     (provides conan, cmake, ninja on PATH)
#
# Qt6 discovery:
#   If $env:QT6_DIR is set, it is used as-is. Otherwise scripts\find_qt6.ps1
#   auto-detects the newest installed C:\Qt\6.*\msvc2022_64 kit.

[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Config = 'Release'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = $PSScriptRoot
$repoRoot = Split-Path -Parent $scriptDir
$logFile = Join-Path $scriptDir "build_log_$($Config.ToLower()).txt"

Start-Transcript -Path $logFile -Force | Out-Null

try {
    Write-Host "=================================================="
    Write-Host "Building sudoku-cpp for Windows ($Config)"
    Write-Host "=================================================="

    # -------------------------------------------------------------------------
    # 1. Locate Visual Studio via vswhere (includes Build Tools and prereleases)
    # -------------------------------------------------------------------------
    $vsInstallerDir = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer'
    $vswhere = Join-Path $vsInstallerDir 'vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found at $vswhere. Install the Visual Studio Installer."
    }

    # vcvarsall.bat (modern VS) internally calls vswhere.exe, so the installer
    # directory must be on PATH before we invoke it.
    $env:PATH = "$vsInstallerDir;$env:PATH"

    $vsPath = & $vswhere -latest -prerelease -products * -property installationPath
    if (-not $vsPath) {
        throw 'No Visual Studio installation found.'
    }
    Write-Host "Found Visual Studio: $vsPath"

    # -------------------------------------------------------------------------
    # 2. Initialize MSVC environment (import vcvarsall.bat env into PS session)
    # -------------------------------------------------------------------------
    $vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
    if (-not (Test-Path $vcvars)) {
        throw "vcvarsall.bat not found at $vcvars"
    }
    $vcvarsOutput = & cmd /c "`"$vcvars`" x64 && set" 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "vcvarsall.bat failed: $vcvarsOutput"
    }
    foreach ($line in $vcvarsOutput) {
        if ($line -match '^([^=]+)=(.*)$') {
            Set-Item -Path "env:$($matches[1])" -Value $matches[2]
        }
    }

    # -------------------------------------------------------------------------
    # 3. Verify Conan is on PATH (expected: activated venv from requirements.txt)
    # -------------------------------------------------------------------------
    $conan = Get-Command conan -ErrorAction SilentlyContinue
    if (-not $conan) {
        throw "conan not found on PATH. Activate the venv first:`n    .\.venv\Scripts\Activate.ps1`nIf you have not set up the venv, see README.md (Windows section)."
    }
    Write-Host "Found Conan: $($conan.Source)"

    # -------------------------------------------------------------------------
    # 4. Resolve Qt6 path
    # -------------------------------------------------------------------------
    if (-not $env:QT6_DIR) {
        $env:QT6_DIR = & (Join-Path $scriptDir 'find_qt6.ps1')
    }
    if (-not $env:QT6_DIR) {
        throw "Qt6 not found. Install the MSVC 2022 64-bit kit via the Qt Online Installer, or set `$env:QT6_DIR manually, e.g.:`n    `$env:QT6_DIR = 'C:\Qt\6.11.1\msvc2022_64'"
    }
    Write-Host "Using Qt6: $env:QT6_DIR"

    Push-Location $repoRoot
    try {
        # ---------------------------------------------------------------------
        # 5. Install Conan dependencies
        #    Pass -s compiler.cppstd=23 explicitly so deps (Catch2, etc.) are
        #    built ABI-compatible with the project's C++23 code, regardless of
        #    what `conan profile detect` defaulted to in the user's profile.
        # ---------------------------------------------------------------------
        Write-Host "[1/3] Installing dependencies with Conan ($Config)..."
        & conan install . `
            --build=missing `
            -s build_type=$Config `
            -s compiler.cppstd=23 `
            --output-folder="build\$Config"
        if ($LASTEXITCODE -ne 0) { throw "Conan dependency installation failed (exit $LASTEXITCODE)" }

        # ---------------------------------------------------------------------
        # 6. Configure CMake (Conan toolchain + Qt6 prefix path)
        # ---------------------------------------------------------------------
        Write-Host "[2/3] Configuring CMake ($Config)..."
        & cmake -S . -B "build\$Config" `
            "-DCMAKE_TOOLCHAIN_FILE=build\$Config\generators\conan_toolchain.cmake" `
            "-DCMAKE_BUILD_TYPE=$Config" `
            "-DCMAKE_PREFIX_PATH=$env:QT6_DIR"
        if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed (exit $LASTEXITCODE)" }

        # ---------------------------------------------------------------------
        # 7. Build
        # ---------------------------------------------------------------------
        Write-Host "[3/3] Building project ($Config)..."
        & cmake --build "build\$Config" --config $Config
        if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)" }

        # ---------------------------------------------------------------------
        # 8. Deploy Qt runtime DLLs via windeployqt
        # ---------------------------------------------------------------------
        $deployFlag = if ($Config -eq 'Debug') { '--debug' } else { '--release' }
        $exePath = "build\$Config\bin\$Config\sudoku.exe"
        if (-not (Test-Path $exePath)) {
            # Single-config generators (Ninja) drop the inner $Config subdir
            $exePath = "build\$Config\bin\sudoku.exe"
        }
        Write-Host "Deploying Qt runtime (windeployqt) for $exePath..."
        & (Join-Path $env:QT6_DIR 'bin\windeployqt.exe') $deployFlag --no-translations $exePath
        if ($LASTEXITCODE -ne 0) { throw "windeployqt failed (exit $LASTEXITCODE)" }
    }
    finally {
        Pop-Location
    }

    Write-Host "=================================================="
    Write-Host "$Config build successful!"
    Write-Host "Executable: $exePath"
    Write-Host "=================================================="
}
finally {
    Stop-Transcript | Out-Null
}
