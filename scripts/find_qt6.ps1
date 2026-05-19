# Discover the newest Qt6 MSVC 2022 64-bit kit under C:\Qt.
# Used by scripts\build_windows.bat and scripts\build_windows_debug.bat.
#
# Prints the absolute path to <Qt-root>\msvc2022_64 on stdout, or nothing
# if no usable kit is found. Exit code is always 0 — callers detect "not
# found" by checking whether stdout is empty.

$ErrorActionPreference = 'Stop'

$root = 'C:\Qt'
if (-not (Test-Path $root)) { return }

$kit = Get-ChildItem $root -Directory -Filter '6.*' -ErrorAction SilentlyContinue |
    Where-Object { Test-Path (Join-Path $_.FullName 'msvc2022_64\lib\cmake\Qt6') } |
    Sort-Object { try { [version]$_.Name } catch { [version]'0.0.0' } } -Descending |
    Select-Object -First 1

if ($kit) {
    Write-Output (Join-Path $kit.FullName 'msvc2022_64')
}
