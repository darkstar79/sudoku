# Run unit and integration tests on Windows.
#
# Usage:
#   .\scripts\run_tests_windows.ps1                  # Release (default)
#   .\scripts\run_tests_windows.ps1 -Config Debug    # Debug

[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Config = 'Release'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = $PSScriptRoot
$repoRoot = Split-Path -Parent $scriptDir

# Multi-config generators (Visual Studio) nest binaries under an extra $Config
# subdir; single-config generators (Ninja) don't.
$candidates = @(
    "$repoRoot\build\$Config\bin\tests\$Config",
    "$repoRoot\build\$Config\bin\tests"
)
$testDir = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $testDir) {
    throw "Test executables not found under $repoRoot\build\$Config\bin\tests. Build first with: .\scripts\build_windows.ps1 -Config $Config"
}

# Test executables link against Qt6 shared libs (translation utilities, etc.)
# but windeployqt only stages DLLs next to sudoku.exe. Prepend Qt6\bin so the
# loader can find Qt6Core.dll etc. without copying DLLs into the test tree.
if (-not $env:QT6_DIR) {
    $env:QT6_DIR = & (Join-Path $scriptDir 'find_qt6.ps1')
}
if ($env:QT6_DIR) {
    $env:PATH = "$env:QT6_DIR\bin;$env:PATH"
}

Write-Host "=================================================="
Write-Host "Running All Tests ($Config)"
Write-Host "=================================================="

$unit = Join-Path $testDir 'unit_tests.exe'
$integ = Join-Path $testDir 'integration_tests.exe'

if (-not (Test-Path $unit))  { throw "unit_tests.exe not found at $unit" }
if (-not (Test-Path $integ)) { throw "integration_tests.exe not found at $integ" }

Write-Host "Running Unit Tests..."
& $unit
if ($LASTEXITCODE -ne 0) { throw "Unit tests failed (exit $LASTEXITCODE)" }

Write-Host ""
Write-Host "Running Integration Tests..."
& $integ
if ($LASTEXITCODE -ne 0) { throw "Integration tests failed (exit $LASTEXITCODE)" }

Write-Host "=================================================="
Write-Host "All tests passed!"
Write-Host "=================================================="
