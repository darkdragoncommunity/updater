param(
    [string]$BuildDir = "out\build\x64-Release",
    [string]$OutputDir = "dist\launcher",
    [switch]$Zip
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildPath = Join-Path $repoRoot $BuildDir
$distPath = Join-Path $repoRoot $OutputDir

if (-not (Test-Path $buildPath)) {
    throw "Build directory not found: $buildPath"
}

$launcherExe = Join-Path $buildPath "launcher.exe"
if (-not (Test-Path $launcherExe)) {
    throw "launcher.exe not found in build directory: $buildPath"
}

if (Test-Path $distPath) {
    Remove-Item $distPath -Recurse -Force
}

New-Item -ItemType Directory -Path $distPath | Out-Null
New-Item -ItemType Directory -Path (Join-Path $distPath "helper") | Out-Null

$runtimeFiles = @(
    "glfw3.dll",
    "libcurl.dll",
    "libcrypto-3-x64.dll",
    "libssl-3-x64.dll",
    "zlib1.dll"
)

foreach ($file in $runtimeFiles) {
    $source = Join-Path $buildPath $file
    if (Test-Path $source) {
        Copy-Item $source $distPath
    }
}

Copy-Item $launcherExe (Join-Path $distPath "launcher.exe")

$bootstrapExe = Join-Path $buildPath "updater_bootstrap.exe"
if (Test-Path $bootstrapExe) {
    Copy-Item $bootstrapExe (Join-Path $distPath "helper\\autoupdate.exe")
}

$assetsSource = Join-Path $repoRoot "assets"
$languagesSource = Join-Path $repoRoot "languages"
$settingsSource = Join-Path $repoRoot "settings.bin"
$configSource = Join-Path $repoRoot "config.json"

if (Test-Path $assetsSource) {
    Copy-Item $assetsSource (Join-Path $distPath "assets") -Recurse
}

if (Test-Path $languagesSource) {
    Copy-Item $languagesSource (Join-Path $distPath "languages") -Recurse
}

if (Test-Path $settingsSource) {
    Copy-Item $settingsSource $distPath
}

if (Test-Path $configSource) {
    Copy-Item $configSource $distPath
}

Write-Host "Packaged launcher to: $distPath"

if ($Zip) {
    $zipPath = "$distPath.zip"
    if (Test-Path $zipPath) {
        Remove-Item $zipPath -Force
    }
    Compress-Archive -Path (Join-Path $distPath "*") -DestinationPath $zipPath -Force
    Write-Host "Created archive: $zipPath"
}
