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
$script:LogFile = Join-Path $scriptDir "build_log_$($Config.ToLower()).txt"

# UTF-8 (no BOM) so the log opens cleanly in any editor / CI tail.
$script:LogEncoding = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText(
    $script:LogFile,
    "=== build_windows.ps1 -Config $Config started at $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') ===`r`n",
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
    # PS 5.1 with $ErrorActionPreference='Stop' converts native-command stderr
    # writes into terminating errors before $LASTEXITCODE is even checked.
    # Conan/cmake routinely write progress to stderr with exit 0, so scope
    # the preference to 'Continue' here and rely on $LASTEXITCODE for the
    # real success signal.
    $oldEap = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $writer = [System.IO.StreamWriter]::new($script:LogFile, $true, $script:LogEncoding)
    try {
        & $ScriptBlock 2>&1 | ForEach-Object {
            # When 2>&1 merges native stderr into the success stream, PS wraps
            # the lines in ErrorRecord objects whose ToString() adds noisy
            # "PS>line:N" framing. Use .TargetObject to recover the original
            # stderr text; fall back to ToString() for everything else.
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
    Write-Log "Building sudoku for Windows ($Config)"
    Write-Log "=================================================="

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
    Write-Log "Found Visual Studio: $vsPath"

    # -------------------------------------------------------------------------
    # 2. Initialize MSVC environment (import vcvarsall.bat env into PS session)
    # -------------------------------------------------------------------------
    $vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
    if (-not (Test-Path $vcvars)) {
        throw "vcvarsall.bat not found at $vcvars"
    }
    $vcvarsOutput = & cmd /c "`"$vcvars`" x64 && set" 2>&1
    if ($LASTEXITCODE -ne 0) {
        [System.IO.File]::AppendAllText(
            $script:LogFile,
            (($vcvarsOutput | ForEach-Object { [string]$_ }) -join "`r`n") + "`r`n",
            $script:LogEncoding
        )
        throw "vcvarsall.bat failed (see $script:LogFile)"
    }
    foreach ($line in $vcvarsOutput) {
        if ($line -match '^([^=]+)=(.*)$') {
            Set-Item -Path "env:$($matches[1])" -Value $matches[2]
        }
    }
    Write-Log "Initialized MSVC environment (vcvarsall.bat x64)"

    # -------------------------------------------------------------------------
    # 3. Verify Conan is on PATH (expected: activated venv from requirements.txt)
    # -------------------------------------------------------------------------
    $conan = Get-Command conan -ErrorAction SilentlyContinue
    if (-not $conan) {
        throw "conan not found on PATH. Activate the venv first:`n    .\.venv\Scripts\Activate.ps1`nIf you have not set up the venv, see README.md (Windows section)."
    }
    Write-Log "Found Conan: $($conan.Source)"

    # -------------------------------------------------------------------------
    # 4. Resolve Qt6 path
    # -------------------------------------------------------------------------
    if (-not $env:QT6_DIR) {
        $env:QT6_DIR = & (Join-Path $scriptDir 'find_qt6.ps1')
    }
    if (-not $env:QT6_DIR) {
        throw "Qt6 not found. Install the MSVC 2022 64-bit kit via the Qt Online Installer, or set `$env:QT6_DIR manually, e.g.:`n    `$env:QT6_DIR = 'C:\Qt\6.11.1\msvc2022_64'"
    }
    Write-Log "Using Qt6: $env:QT6_DIR"

    Push-Location $repoRoot
    try {
        # ---------------------------------------------------------------------
        # 5. Install Conan dependencies
        #    Pass -s compiler.cppstd=23 explicitly so deps (Catch2, etc.) are
        #    built ABI-compatible with the project's C++23 code, regardless of
        #    what `conan profile detect` defaulted to in the user's profile.
        # ---------------------------------------------------------------------
        Invoke-LoggedStep "[1/3] Installing dependencies with Conan ($Config)..." {
            conan install . `
                --build=missing `
                -s build_type=$Config `
                -s compiler.cppstd=23 `
                --output-folder="build\$Config"
        }

        # ---------------------------------------------------------------------
        # 6. Configure CMake (Conan toolchain + Qt6 prefix path)
        # ---------------------------------------------------------------------
        Invoke-LoggedStep "[2/3] Configuring CMake ($Config)..." {
            cmake -S . -B "build\$Config" `
                "-DCMAKE_TOOLCHAIN_FILE=build\$Config\generators\conan_toolchain.cmake" `
                "-DCMAKE_BUILD_TYPE=$Config" `
                "-DCMAKE_PREFIX_PATH=$env:QT6_DIR"
        }

        # ---------------------------------------------------------------------
        # 7. Build
        # ---------------------------------------------------------------------
        Invoke-LoggedStep "[3/3] Building project ($Config)..." {
            cmake --build "build\$Config" --config $Config
        }

        # ---------------------------------------------------------------------
        # 8. Deploy Qt runtime DLLs via windeployqt
        # ---------------------------------------------------------------------
        $deployFlag = if ($Config -eq 'Debug') { '--debug' } else { '--release' }
        $exePath = "build\$Config\bin\$Config\sudoku.exe"
        if (-not (Test-Path $exePath)) {
            # Single-config generators (Ninja) drop the inner $Config subdir
            $exePath = "build\$Config\bin\sudoku.exe"
        }
        $windeployqt = Join-Path $env:QT6_DIR 'bin\windeployqt.exe'
        Invoke-LoggedStep "Deploying Qt runtime (windeployqt) for $exePath..." {
            & $windeployqt $deployFlag --no-translations $exePath
        }
    }
    finally {
        Pop-Location
    }

    Write-Log "=================================================="
    Write-Log "$Config build successful!"
    Write-Log "Executable: $exePath"
    Write-Log "=================================================="
}
catch {
    Write-Log "ERROR: $($_.Exception.Message)"
    throw
}
