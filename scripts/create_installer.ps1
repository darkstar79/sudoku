# Build a Windows installer (NSIS via CPack).
#
# Usage:
#   .\scripts\create_installer.ps1
#
# Prerequisites:
#   - NSIS installed (winget install NSIS.NSIS)
#   - Visual Studio with the cmake bundled under
#       <VS>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin
#     ConanCenter's cmake is currently pinned to 3.31 which doesn't know the
#     "Visual Studio 18 2026" generator; the bundled cmake is 4.x and does.
#
# This script delegates the actual build to build_windows.ps1 (Release) and
# then runs `cmake -E chdir build\Release cpack -G NSIS`.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = $PSScriptRoot
$repoRoot = Split-Path -Parent $scriptDir
$logFile = Join-Path $scriptDir 'installer_log.txt'

Start-Transcript -Path $logFile -Force | Out-Null

try {
    Write-Host "=================================================="
    Write-Host " Sudoku Windows Installer Builder"
    Write-Host "=================================================="

    # -------------------------------------------------------------------------
    # 1. Locate NSIS (also check default install location if not on PATH)
    # -------------------------------------------------------------------------
    $makensis = Get-Command makensis.exe -ErrorAction SilentlyContinue
    if (-not $makensis) {
        $nsisDefault = Join-Path ${env:ProgramFiles(x86)} 'NSIS'
        if (Test-Path (Join-Path $nsisDefault 'makensis.exe')) {
            $env:PATH = "$nsisDefault;$env:PATH"
            $makensis = Get-Command makensis.exe
        }
    }
    if (-not $makensis) {
        throw "NSIS not found. Install with one of:`n    winget install NSIS.NSIS`n    choco install nsis`nThen ensure makensis.exe is on your PATH."
    }
    Write-Host "Found NSIS: $($makensis.Source)"

    # -------------------------------------------------------------------------
    # 2. Locate the cmake bundled with Visual Studio (4.x — needed for the
    #    "Visual Studio 18 2026" generator that CPackConfig references).
    # -------------------------------------------------------------------------
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found at $vswhere"
    }
    $vsPath = & $vswhere -latest -prerelease -products * -property installationPath
    if (-not $vsPath) {
        throw 'No Visual Studio installation found.'
    }
    $vsCMakeBin = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
    $vsCMake = Join-Path $vsCMakeBin 'cmake.exe'
    if (-not (Test-Path $vsCMake)) {
        throw "cmake not found in VS installation at $vsCMakeBin"
    }
    Write-Host "Found VS cmake: $vsCMake"

    # -------------------------------------------------------------------------
    # 3. Release build (incremental — skips if already up to date)
    # -------------------------------------------------------------------------
    Write-Host "[1/2] Building Release..."
    & (Join-Path $scriptDir 'build_windows.ps1') -Config Release
    if ($LASTEXITCODE -ne 0) {
        throw "Release build failed. See $($scriptDir)\build_log_release.txt for details."
    }

    # -------------------------------------------------------------------------
    # 4. CPack: stage install tree + run NSIS
    # -------------------------------------------------------------------------
    Write-Host "[2/2] Packaging installer..."
    $env:PATH = "$vsCMakeBin;$env:PATH"
    & $vsCMake -E chdir (Join-Path $repoRoot 'build\Release') cpack -G NSIS -C Release
    if ($LASTEXITCODE -ne 0) {
        throw "CPack/NSIS packaging failed (exit $LASTEXITCODE)."
    }

    # Move installer to project root
    $installers = Get-ChildItem -Path (Join-Path $repoRoot 'build\Release') -Filter 'Sudoku-*-win64.exe' -ErrorAction SilentlyContinue
    foreach ($installer in $installers) {
        Move-Item -Path $installer.FullName -Destination $repoRoot -Force
    }

    Write-Host "=================================================="
    Write-Host "Installer ready!"
    Get-ChildItem -Path $repoRoot -Filter 'Sudoku-*-win64.exe' | ForEach-Object { Write-Host "Installer: $($_.Name)" }
    Write-Host "=================================================="
}
finally {
    Stop-Transcript | Out-Null
}
