# ============================================================================
# CRAY LDR URBAN INSTRUMENTS — Release Packaging Script
# scripts\package_release.ps1
#
# Usage:
#   cd <repo-root>
#   .\scripts\package_release.ps1
#
# Optional parameters:
#   -JuceDir     "C:\path\to\JUCE"   (skip FetchContent download)
#   -Version     "1.0.0"             (override version string)
#   -BuildType   "Release"
#   -OutDir      ".\dist"
#
# Requirements:
#   • CMake 3.22+ on PATH
#   • Visual Studio 2022 (VS17 2022) installed
#   • 7-Zip OR the PowerShell Compress-Archive fallback
# ============================================================================

[CmdletBinding()]
param(
    [string] $JuceDir   = "",
    [string] $Version   = "1.0.0",
    [string] $BuildType = "Release",
    [string] $OutDir    = ".\dist"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
function Write-Step([string] $msg) {
    Write-Host ""
    Write-Host "==> $msg" -ForegroundColor Cyan
}

function Assert-Command([string] $cmd) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
        Write-Error "$cmd not found on PATH. Please install it and retry."
    }
}

# ---------------------------------------------------------------------------
# 0. Pre-flight checks
# ---------------------------------------------------------------------------
Write-Step "Pre-flight checks"

Assert-Command "cmake"
$cmakeVer = (cmake --version | Select-Object -First 1) -replace "cmake version ",""
Write-Host "   CMake: $cmakeVer"

$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vsWhere)) {
    Write-Error "Visual Studio 2022 not found. Install it with the 'Desktop development with C++' workload."
}
$vsPath = & $vsWhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
Write-Host "   VS2022: $vsPath"

# ---------------------------------------------------------------------------
# 1. Resolve root directory (script lives in scripts\)
# ---------------------------------------------------------------------------
$root = (Resolve-Path "$PSScriptRoot\..").Path
Write-Step "Repository root: $root"

$buildDir  = Join-Path $root "build_release"
$stageDir  = Join-Path $root "staging"
$outDir    = (Join-Path $root $OutDir)
$zipName   = "CRAY_LDR_Urban_Instruments_v${Version}_Win64.zip"
$zipPath   = Join-Path $outDir $zipName

# Clean previous staging
if (Test-Path $stageDir) { Remove-Item $stageDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null
New-Item -ItemType Directory -Force -Path $outDir  | Out-Null

# ---------------------------------------------------------------------------
# 2. CMake Configure
# ---------------------------------------------------------------------------
Write-Step "Configuring CMake ($BuildType, x64)"

$cmakeArgs = @(
    "-B", $buildDir,
    "-G", "Visual Studio 17 2022",
    "-A", "x64",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCRAY_BUILD_TESTS=ON",
    "-DCRAY_BUILD_STANDALONE=ON"
)

if ($JuceDir -ne "" -and (Test-Path $JuceDir)) {
    $cmakeArgs += @("-DJUCE_DIR=$JuceDir")
    Write-Host "   Using local JUCE at: $JuceDir"
} else {
    Write-Host "   JUCE will be fetched via FetchContent (may take a few minutes on first run)"
}

& cmake $root @cmakeArgs
if ($LASTEXITCODE -ne 0) { Write-Error "CMake configure failed." }

# ---------------------------------------------------------------------------
# 3. Build
# ---------------------------------------------------------------------------
Write-Step "Building $BuildType"

& cmake --build $buildDir --config $BuildType --parallel
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed." }

# ---------------------------------------------------------------------------
# 4. Run Tests
# ---------------------------------------------------------------------------
Write-Step "Running unit tests"

Push-Location $buildDir
& ctest -C $BuildType --output-on-failure --timeout 120
$testResult = $LASTEXITCODE
Pop-Location

if ($testResult -ne 0) {
    Write-Warning "Some unit tests failed! Packaging anyway — please fix before distribution."
}

# ---------------------------------------------------------------------------
# 5. Locate VST3 bundle
# ---------------------------------------------------------------------------
Write-Step "Locating VST3 artefact"

$vst3Search = Get-ChildItem -Recurse -Filter "*.vst3" $buildDir `
              | Where-Object { $_.PSIsContainer } `
              | Select-Object -First 1

if (-not $vst3Search) {
    # Also check as a file (single-file VST3)
    $vst3Search = Get-ChildItem -Recurse -Filter "*.vst3" $buildDir `
                  | Select-Object -First 1
}

if (-not $vst3Search) {
    Write-Error "Could not find .vst3 in build output. Did the build succeed?"
}
Write-Host "   VST3: $($vst3Search.FullName)"

# ---------------------------------------------------------------------------
# 6. Stage files
# ---------------------------------------------------------------------------
Write-Step "Staging release files"

# VST3
$vst3Dest = Join-Path $stageDir "VST3"
New-Item -ItemType Directory -Force -Path $vst3Dest | Out-Null
if ($vst3Search.PSIsContainer) {
    # Bundle directory
    Copy-Item $vst3Search.FullName $vst3Dest -Recurse
} else {
    Copy-Item $vst3Search.FullName $vst3Dest
}

# Resources
$resSrc  = Join-Path $root "Resources"
$resDest = Join-Path $stageDir "Resources"
Copy-Item $resSrc $resDest -Recurse

# Standalone (optional)
$standAloneSearch = Get-ChildItem -Recurse -Filter "*.exe" $buildDir `
                    | Where-Object { $_.Name -like "*CRAY*" -or $_.Name -like "*Urban*" } `
                    | Select-Object -First 1
if ($standAloneSearch) {
    $standaloneDest = Join-Path $stageDir "Standalone"
    New-Item -ItemType Directory -Force -Path $standaloneDest | Out-Null
    Copy-Item $standAloneSearch.FullName $standaloneDest
    Write-Host "   Standalone: $($standAloneSearch.FullName)"
}

# Docs
Copy-Item (Join-Path $root "README.md") $stageDir
Copy-Item (Join-Path $root "docs")      $stageDir -Recurse

# Version file
@"
CRAY LDR URBAN INSTRUMENTS
Version: $Version
Build: $BuildType
Packaged: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
Platform: Windows x64
"@ | Set-Content (Join-Path $stageDir "VERSION.txt")

# ---------------------------------------------------------------------------
# 7. Create ZIP
# ---------------------------------------------------------------------------
Write-Step "Creating ZIP: $zipName"

if (Test-Path $zipPath) { Remove-Item $zipPath -Force }

# Prefer 7-Zip for better compression, fallback to PowerShell built-in
$sevenZip = Get-Command "7z" -ErrorAction SilentlyContinue

if ($sevenZip) {
    & 7z a -tzip "$zipPath" "$stageDir\*" -mx=5
} else {
    Compress-Archive -Path "$stageDir\*" -DestinationPath $zipPath -CompressionLevel Optimal
}

if (-not (Test-Path $zipPath)) {
    Write-Error "ZIP creation failed."
}

$zipSize = [math]::Round((Get-Item $zipPath).Length / 1MB, 2)
Write-Step "Package complete"
Write-Host ""
Write-Host "   Output : $zipPath" -ForegroundColor Green
Write-Host "   Size   : ${zipSize} MB"      -ForegroundColor Green
Write-Host ""
Write-Host "To install, extract the ZIP and copy VST3\ contents to:"
Write-Host "   C:\Program Files\Common Files\VST3\"
Write-Host "Then copy Resources\ alongside the .vst3 folder."
Write-Host ""
