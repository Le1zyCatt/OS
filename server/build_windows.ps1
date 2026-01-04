# Build and run Windows native real filesystem server
# Usage: In the repository root PowerShell run:
#   powershell -ExecutionPolicy Bypass -File .\server\build_windows.ps1

[CmdletBinding()]
param(
    [switch]$BuildOnly,
    [switch]$Clean,
    [string]$BuildType = 'Release'
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path "$PSScriptRoot\..").Path
$serverDir = Join-Path $repoRoot 'server'
$buildDir = Join-Path $serverDir 'build_win'
$diskDir = Join-Path $repoRoot 'filesystem\disk'

Write-Host "=======================================" -ForegroundColor Cyan
Write-Host "Windows Real FS Server Build Script" -ForegroundColor Cyan
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host ""

# Ensure disk directory exists
if (!(Test-Path $diskDir)) {
    Write-Host "[INFO] Creating disk directory: $diskDir" -ForegroundColor Yellow
    New-Item -ItemType Directory -Path $diskDir -Force | Out-Null
}

# Clean build directory
if ($Clean) {
    Write-Host "[1/4] Cleaning build directory..." -ForegroundColor Cyan
    if (Test-Path $buildDir) {
        Remove-Item -Recurse -Force $buildDir
    }
}

# Create build directory
if (!(Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
}

# Run CMake configuration
Write-Host "[2/4] Running CMake configuration..." -ForegroundColor Cyan
Push-Location $buildDir

try {
    # Configure project with Visual Studio generator (more reliable on Windows)
    # Use -G to specify generator, avoiding MinGW/Ninja conflicts
    $vsGenerator = "Visual Studio 17 2022"
    
    # Try VS 2022 first, fall back to VS 2019
    $testResult = cmake --help 2>&1 | Select-String "Visual Studio 17 2022"
    if (-not $testResult) {
        $vsGenerator = "Visual Studio 16 2019"
    }
    
    Write-Host "  Using generator: $vsGenerator" -ForegroundColor Gray
    cmake .. -G $vsGenerator -DUSE_REAL_FILESYSTEM=ON -DCMAKE_BUILD_TYPE=$BuildType
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }

    # Build project
    Write-Host "[3/4] Building project..." -ForegroundColor Cyan
    cmake --build . --config $BuildType
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }

    Write-Host "[OK] Build completed successfully!" -ForegroundColor Green
} finally {
    Pop-Location
}

if ($BuildOnly) {
    Write-Host ""
    Write-Host "Build-only mode. Server not started." -ForegroundColor Yellow
    Write-Host "To run the server manually:" -ForegroundColor Yellow
    Write-Host "  cd $buildDir\$BuildType" -ForegroundColor White
    Write-Host "  .\server.exe" -ForegroundColor White
    exit 0
}

# Run server
Write-Host "[4/4] Starting server..." -ForegroundColor Cyan

$serverExe = Join-Path $buildDir "$BuildType\server.exe"
if (!(Test-Path $serverExe)) {
    # Try alternative path
    $serverExe = Join-Path $buildDir "server.exe"
}

if (!(Test-Path $serverExe)) {
    Write-Host "[ERR] Server executable not found!" -ForegroundColor Red
    Write-Host "  Expected: $serverExe" -ForegroundColor Red
    exit 1
}

Write-Host "  Server: $serverExe" -ForegroundColor Green
Write-Host ""
Write-Host "Press Ctrl+C to stop the server" -ForegroundColor Yellow
Write-Host ""

# Switch to build directory to run (so relative paths can find disk.img)
Push-Location (Split-Path $serverExe -Parent)
try {
    & $serverExe
} finally {
    Pop-Location
}
