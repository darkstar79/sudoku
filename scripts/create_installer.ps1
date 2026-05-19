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
$script:LogFile = Join-Path $scriptDir 'installer_log.txt'

# UTF-8 (no BOM) so the log opens cleanly in any editor / CI tail.
$script:LogEncoding = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText(
    $script:LogFile,
    "=== create_installer.ps1 started at $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') ===`r`n",
    $script:LogEncoding
)

function Write-Log {
    param([Parameter(Mandatory, ValueFromPipeline)][string]$Message)
    process {
        Write-Host $Message
        [System.IO.File]::AppendAllText($script:LogFile, "$Message`r`n", $script:LogEncoding)
    }
}

function Invoke-LoggedStep {
    param(
        [Parameter(Mandatory)][string]$Description,
        [Parameter(Mandatory)][scriptblock]$ScriptBlock
    )
    Write-Log $Description
    # PS 5.1 with $ErrorActionPreference='Stop' treats native-command stderr
    # writes as terminating errors; relax to 'Continue' here and rely on
    # $LASTEXITCODE for the real success signal.
    $oldEap = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $writer = [System.IO.StreamWriter]::new($script:LogFile, $true, $script:LogEncoding)
    try {
        & $ScriptBlock 2>&1 | ForEach-Object {
            $line = if ($_ -is [System.Management.Automation.ErrorRecord] -and $null -ne $_.TargetObject) {
                [string]$_.TargetObject
            } else {
                [string]$_
            }
            Write-Host $line
            $writer.WriteLine($line)
        }
    }
    finally {
        $writer.Dispose()
        $ErrorActionPreference = $oldEap
    }
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed (exit $LASTEXITCODE)"
    }
}

try {
    Write-Log "=================================================="
    Write-Log " Sudoku Windows Installer Builder"
    Write-Log "=================================================="

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
    Write-Log "Found NSIS: $($makensis.Source)"

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
    Write-Log "Found VS cmake: $vsCMake"

    # -------------------------------------------------------------------------
    # 3. Release build (incremental — skips if already up to date)
    # -------------------------------------------------------------------------
    Invoke-LoggedStep "[1/2] Building Release..." {
        & (Join-Path $scriptDir 'build_windows.ps1') -Config Release
    }

    # -------------------------------------------------------------------------
    # 4. CPack: stage install tree + run NSIS
    # -------------------------------------------------------------------------
    $env:PATH = "$vsCMakeBin;$env:PATH"
    Invoke-LoggedStep "[2/2] Packaging installer..." {
        & $vsCMake -E chdir (Join-Path $repoRoot 'build\Release') cpack -G NSIS -C Release
    }

    # Move installer to project root
    $installers = Get-ChildItem -Path (Join-Path $repoRoot 'build\Release') -Filter 'Sudoku-*-win64.exe' -ErrorAction SilentlyContinue
    foreach ($installer in $installers) {
        Move-Item -Path $installer.FullName -Destination $repoRoot -Force
    }

    Write-Log "=================================================="
    Write-Log "Installer ready!"
    Get-ChildItem -Path $repoRoot -Filter 'Sudoku-*-win64.exe' | ForEach-Object {
        Write-Log "Installer: $($_.Name)"
    }
    Write-Log "=================================================="
}
catch {
    Write-Log "ERROR: $($_.Exception.Message)"
    throw
}
