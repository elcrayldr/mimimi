#requires -Version 5.1
# ============================================================================
# CRAY LDR URBAN INSTRUMENTS — Windows x64 Release Packaging
#
# Run from any directory:
#   .\package_release.ps1
#
# Optional:
#   -JuceDir "C:\SDK\JUCE"
#   -Version "1.0.2"
#   -BuildType Release
#   -OutDir ".\dist"
#
# Requirements: Windows 10/11, PowerShell 5.1+, CMake 3.22+,
# Visual Studio 2022 C++ workload, Python 3, and 7-Zip (optional).
# ============================================================================

[CmdletBinding()]
param(
    [string] $JuceDir = "",
    [string] $Version = "1.0.2",
    [ValidateSet("Release", "RelWithDebInfo")]
    [string] $BuildType = "Release",
    [string] $OutDir = ".\dist"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Write-Step([string] $Message) {
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Assert-Command([string] $Name) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "$Name was not found on PATH."
    }
}

$root = $PSScriptRoot
$buildDir = Join-Path $root "build_release"
$stageDir = Join-Path $root "staging"
if ([System.IO.Path]::IsPathRooted($OutDir)) {
    $outDirPath = [System.IO.Path]::GetFullPath($OutDir)
} else {
    $outDirPath = [System.IO.Path]::GetFullPath((Join-Path $root $OutDir))
}
$zipName = "CRAY_LDR_Urban_Instruments_v${Version}_Win64.zip"
$zipPath = Join-Path $outDirPath $zipName

Write-Step "Checking build tools"
Assert-Command "cmake"
Assert-Command "python"

$vsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vsWhere)) {
    throw "Visual Studio Installer (vswhere.exe) was not found. Install Visual Studio 2022 with Desktop development with C++."
}
$vsPath = & $vsWhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw "A Visual Studio installation with the x64 C++ toolchain was not found." }
Write-Host "Visual Studio: $vsPath"

if (-not (Test-Path -LiteralPath (Join-Path $root "CMakeLists.txt"))) {
    throw "CMakeLists.txt was not found in repository root: $root"
}

Write-Step "Configuring CMake (x64 / $BuildType)"
$cmakeArgs = @(
    "-S", $root,
    "-B", $buildDir,
    "-G", "Visual Studio 17 2022",
    "-A", "x64",
    "-DCRAY_BUILD_TESTS=ON",
    "-DCRAY_BUILD_STANDALONE=ON"
)
if ($JuceDir) {
    if (-not (Test-Path -LiteralPath (Join-Path $JuceDir "CMakeLists.txt"))) {
        throw "JuceDir does not contain JUCE's CMakeLists.txt: $JuceDir"
    }
    $cmakeArgs += "-DJUCE_DIR=$JuceDir"
}
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed with exit code $LASTEXITCODE." }

Write-Step "Building VST3 and Standalone"
& cmake --build $buildDir --config $BuildType --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE." }

Write-Step "Running automated tests"
& ctest --test-dir $buildDir -C $BuildType --output-on-failure --timeout 120
if ($LASTEXITCODE -ne 0) { throw "Unit tests failed. Packaging has been stopped." }

Write-Step "Validating VST3 bundle and factory presets"
$vst3 = Get-ChildItem -LiteralPath $buildDir -Recurse -Directory -Filter "*.vst3" |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "Contents\x86_64-win") } |
    Select-Object -First 1
if (-not $vst3) { throw "A Windows x64 VST3 bundle was not found in the build output." }

$presetDir = Join-Path $vst3.FullName "Contents\Resources\FactoryPresets"
& python (Join-Path $root "scripts\validate_factory_presets.py") $presetDir
if ($LASTEXITCODE -ne 0) { throw "Factory preset validation failed." }
& python (Join-Path $root "scripts\validate_vst3_bundle.py") $vst3.FullName
if ($LASTEXITCODE -ne 0) { throw "VST3 bundle validation failed." }

Write-Step "Preparing release package"
if (Test-Path -LiteralPath $stageDir) {
    Remove-Item -LiteralPath $stageDir -Recurse -Force
}
New-Item -ItemType Directory -Path $stageDir -Force | Out-Null
New-Item -ItemType Directory -Path $outDirPath -Force | Out-Null

$vst3Stage = Join-Path $stageDir "VST3"
New-Item -ItemType Directory -Path $vst3Stage -Force | Out-Null
Copy-Item -LiteralPath $vst3.FullName -Destination $vst3Stage -Recurse

foreach ($doc in @("README.md", "ARCHITECTURE.md")) {
    $source = Join-Path $root $doc
    if (Test-Path -LiteralPath $source) {
        Copy-Item -LiteralPath $source -Destination $stageDir
    }
}

# Include the Standalone executable when the configured target produced one.
$standalone = Get-ChildItem -LiteralPath $buildDir -Recurse -File -Filter "*.exe" |
    Where-Object { $_.Name -like "*CRAY*" -or $_.Name -like "*Urban*" } |
    Select-Object -First 1
if ($standalone) {
    $standaloneStage = Join-Path $stageDir "Standalone"
    New-Item -ItemType Directory -Path $standaloneStage -Force | Out-Null
    Copy-Item -LiteralPath $standalone.FullName -Destination $standaloneStage
}

@"
CRAY LDR URBAN INSTRUMENTS
Version: $Version
Build: $BuildType
Packaged: $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")
Platform: Windows x64
JUCE: 8.0.6
"@ | Set-Content -LiteralPath (Join-Path $stageDir "VERSION.txt") -Encoding UTF8

Write-Step "Creating ZIP archive"
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
$sevenZip = Get-Command "7z" -ErrorAction SilentlyContinue
if ($sevenZip) {
    & $sevenZip.Source a -tzip $zipPath (Join-Path $stageDir "*") -mx=5 | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "7-Zip failed with exit code $LASTEXITCODE." }
} else {
    Compress-Archive -Path (Join-Path $stageDir "*") -DestinationPath $zipPath -CompressionLevel Optimal
}
if (-not (Test-Path -LiteralPath $zipPath)) { throw "ZIP archive was not created." }

$zipSize = [math]::Round((Get-Item -LiteralPath $zipPath).Length / 1MB, 2)
Write-Step "Release package created"
Write-Host "File: $zipPath" -ForegroundColor Green
Write-Host "Size: $zipSize MB" -ForegroundColor Green
Write-Host "Install the complete .vst3 folder from VST3 into:"
Write-Host "  C:\Program Files\Common Files\VST3"
Write-Host "Then rescan VST3 plugins in your DAW."
