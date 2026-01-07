# One-click start: Real filesystem Server + Windows Web Console
# Supports Windows native build and WSL modes
# Usage: Run in PowerShell from repo root:
#   powershell -ExecutionPolicy Bypass -File .\start_realfs_web.ps1

[CmdletBinding()]
param(
    [int]$BackendPort = 8080,
    [int]$WebPort = 5000,
    [string]$WebHost = '127.0.0.1',
    [switch]$UseWSL
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path $PSScriptRoot).Path

# Print startup banner
function Show-Banner {
    Write-Host ""
    Write-Host "=================================================================" -ForegroundColor Cyan
    Write-Host "    Academic Paper Review System - Quick Start Script" -ForegroundColor Cyan
    Write-Host "=================================================================" -ForegroundColor Cyan
    Write-Host "  This script will:" -ForegroundColor White
    Write-Host "    1. Detect/start C++ backend server (port $BackendPort)" -ForegroundColor White
    Write-Host "    2. Detect/start Python Web console (port $WebPort)" -ForegroundColor White
    Write-Host "    3. Open browser to access Web UI" -ForegroundColor White
    Write-Host "=================================================================" -ForegroundColor Cyan
    Write-Host ""
}

Show-Banner

function Test-PortOpen {
    param([string]$HostName, [int]$Port)
    try {
        $r = Test-NetConnection -ComputerName $HostName -Port $Port -WarningAction SilentlyContinue
        return [bool]$r.TcpTestSucceeded
    } catch {
        return $false
    }
}

function Get-WslIPv4 {
    try {
        $ip = (wsl sh -lc "hostname -I 2>/dev/null | tr ' ' '\n' | head -n1" 2>$null).Trim()
        if ($ip -match '^(\d{1,3}\.){3}\d{1,3}$') { return $ip }
        return $null
    } catch {
        return $null
    }
}

function Wait-Port {
    param([string]$HostName, [int]$Port, [int]$TimeoutSec = 30)
    $deadline = (Get-Date).AddSeconds($TimeoutSec)
    while ((Get-Date) -lt $deadline) {
        if (Test-PortOpen -HostName $HostName -Port $Port) { return $true }
        Start-Sleep -Seconds 1
    }
    return $false
}

Write-Host "-----------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "[Step 1/4] Checking C++ Backend Server..." -ForegroundColor Cyan
Write-Host "-----------------------------------------------------------------" -ForegroundColor DarkGray
$wslIp = Get-WslIPv4
$backendHost = $null

if (Test-PortOpen -HostName 'localhost' -Port $BackendPort) {
    $backendHost = 'localhost'
    Write-Host "  [OK] Backend server already running" -ForegroundColor Green
    Write-Host "       Address: localhost:$BackendPort" -ForegroundColor Gray
} elseif ($wslIp -and (Test-PortOpen -HostName $wslIp -Port $BackendPort)) {
    $backendHost = $wslIp
    Write-Host "  [OK] Backend server running in WSL" -ForegroundColor Green
    Write-Host "       Address: ${wslIp}:$BackendPort" -ForegroundColor Gray
} else {
    if ($UseWSL) {
        Write-Host "  [INFO] Backend not detected" -ForegroundColor Yellow
        Write-Host "  [BUILD] Starting WSL server..." -ForegroundColor Yellow

        $runServerScript = Join-Path $repoRoot 'server\run_realfs_wsl.ps1'
        if (!(Test-Path $runServerScript)) {
            throw ('Script not found: {0}' -f $runServerScript)
        }

        Start-Process powershell.exe -WorkingDirectory $repoRoot -ArgumentList @(
            '-NoExit',
            '-ExecutionPolicy', 'Bypass',
            '-File', $runServerScript
        )
    } else {
        Write-Host "  [INFO] Backend not detected" -ForegroundColor Yellow
        Write-Host "  [BUILD] Compiling Windows native server..." -ForegroundColor Yellow
        Write-Host "          (First build may take 30-60 seconds)" -ForegroundColor Gray

        $buildScript = Join-Path $repoRoot 'server\build_windows.ps1'
        if (!(Test-Path $buildScript)) {
            throw ('Script not found: {0}' -f $buildScript)
        }

        Start-Process powershell.exe -WorkingDirectory $repoRoot -ArgumentList @(
            '-NoExit',
            '-ExecutionPolicy', 'Bypass',
            '-File', $buildScript
        )
    }

    Write-Host "  [WAIT] Waiting for backend (max 60s)..." -ForegroundColor Yellow
    if (Wait-Port -HostName 'localhost' -Port $BackendPort -TimeoutSec 60) {
        $backendHost = 'localhost'
        Write-Host "  [OK] Backend server started successfully" -ForegroundColor Green
        Write-Host "       Address: localhost:$BackendPort" -ForegroundColor Gray
    } elseif ($wslIp -and (Wait-Port -HostName $wslIp -Port $BackendPort -TimeoutSec 10)) {
        $backendHost = $wslIp
        Write-Host "  [OK] Backend server started (WSL)" -ForegroundColor Green
        Write-Host "       Address: ${wslIp}:$BackendPort" -ForegroundColor Gray
    } else {
        Write-Host "  [ERROR] Backend server failed to start!" -ForegroundColor Red
        Write-Host "          Check the Server window for errors" -ForegroundColor Red
        Write-Host "          Tip: Ensure CMake and C++ compiler are installed" -ForegroundColor Yellow
        Write-Host "          Or use WSL mode: .\start_realfs_web.ps1 -UseWSL" -ForegroundColor Yellow
        exit 1
    }
}
Write-Host ""

Write-Host "-----------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "[Step 2/4] Checking Python Web Console..." -ForegroundColor Cyan
Write-Host "-----------------------------------------------------------------" -ForegroundColor DarkGray
if (Test-PortOpen -HostName 'localhost' -Port $WebPort) {
    Write-Host "  [OK] Web console already running" -ForegroundColor Green
    Write-Host "       Address: http://${WebHost}:$WebPort" -ForegroundColor Gray
} else {
    Write-Host "  [CHECK] Checking Flask dependency..." -ForegroundColor Yellow
    try {
        python -c "import flask" 2>$null | Out-Null
        Write-Host "          Flask is installed" -ForegroundColor Gray
    } catch {
        Write-Host "          Installing Flask..." -ForegroundColor Yellow
        python -m pip install flask --quiet
        Write-Host "          Flask installed" -ForegroundColor Gray
    }

    Write-Host "  [START] Starting Web console..." -ForegroundColor Yellow
    $webDir = Join-Path $repoRoot 'server\web'
    Start-Process powershell.exe -WorkingDirectory $webDir -ArgumentList @(
        '-NoExit',
        '-Command',
        ('python web_server.py --host {0} --port {1} --backend-host {2} --backend-port {3}' -f $WebHost, $WebPort, $backendHost, $BackendPort)
    )

    Write-Host "  [WAIT] Waiting for Web console (max 15s)..." -ForegroundColor Yellow
    if (!(Wait-Port -HostName 'localhost' -Port $WebPort -TimeoutSec 15)) {
        Write-Host "  [ERROR] Web console failed to start!" -ForegroundColor Red
        Write-Host "          Check the Web window for errors" -ForegroundColor Red
        exit 1
    }

    Write-Host "  [OK] Web console started successfully" -ForegroundColor Green
    Write-Host "       Address: http://${WebHost}:$WebPort" -ForegroundColor Gray
}
Write-Host ""

Write-Host "-----------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "[Step 3/4] Opening Browser..." -ForegroundColor Cyan
Write-Host "-----------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "  [OPEN] Opening default browser..." -ForegroundColor Yellow
Start-Process ("http://{0}:{1}" -f $WebHost, $WebPort)
Write-Host "  [OK] Browser opened" -ForegroundColor Green
Write-Host ""

Write-Host "-----------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "[Step 4/4] Startup Complete!" -ForegroundColor Cyan
Write-Host "-----------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host ""
Write-Host "=================================================================" -ForegroundColor Green
Write-Host "              System Started Successfully!" -ForegroundColor Green
Write-Host "=================================================================" -ForegroundColor Green
Write-Host "  Backend Server: $backendHost`:$BackendPort" -ForegroundColor White
Write-Host "  Web Console:    http://${WebHost}:$WebPort" -ForegroundColor White
Write-Host "-----------------------------------------------------------------" -ForegroundColor Green
Write-Host "  Usage:" -ForegroundColor White
Write-Host "    - Click user cards on the left to login" -ForegroundColor Gray
Write-Host "      (admin/editor/reviewer/author)" -ForegroundColor Gray
Write-Host "    - Click 'Run All Tests' to try all features" -ForegroundColor Gray
Write-Host "    - Use 'Snapshot' to save/restore filesystem state" -ForegroundColor Gray
Write-Host "=================================================================" -ForegroundColor Green
Write-Host ""