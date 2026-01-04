# 一键启动：真实文件系统 Server + Windows Web 控制台
# 支持 Windows 原生编译和 WSL 两种模式
# 用法：在仓库根目录 PowerShell 运行：
#   powershell -ExecutionPolicy Bypass -File .\start_realfs_web.ps1

[CmdletBinding()]
param(
    [int]$BackendPort = 8080,
    [int]$WebPort = 5000,
    [string]$WebHost = '127.0.0.1',
    [switch]$UseWSL  # 使用 WSL 运行服务器（旧模式）
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path $PSScriptRoot).Path

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

Write-Host '[1/4] Check backend server (real FS) ...' -ForegroundColor Cyan
$wslIp = Get-WslIPv4
$backendHost = $null

if (Test-PortOpen -HostName 'localhost' -Port $BackendPort) {
    $backendHost = 'localhost'
    Write-Host ("  [OK] Backend listening at localhost:{0}" -f $BackendPort) -ForegroundColor Green
} elseif ($wslIp -and (Test-PortOpen -HostName $wslIp -Port $BackendPort)) {
    $backendHost = $wslIp
    Write-Host ("  [OK] Backend listening at WSL {0}:{1}" -f $wslIp, $BackendPort) -ForegroundColor Green
} else {
    if ($UseWSL) {
        # WSL 模式：使用 WSL 运行服务器
        Write-Host "  [INFO] Backend not detected; starting WSL real-FS server" -ForegroundColor Yellow

        $runServerScript = Join-Path $repoRoot 'server\run_realfs_wsl.ps1'
        if (!(Test-Path $runServerScript)) {
            throw ('找不到脚本: {0}' -f $runServerScript)
        }

        Start-Process powershell.exe -WorkingDirectory $repoRoot -ArgumentList @(
            '-NoExit',
            '-ExecutionPolicy', 'Bypass',
            '-File', $runServerScript
        )
    } else {
        # Windows 原生模式：编译并运行 Windows 版本
        Write-Host "  [INFO] Backend not detected; building Windows native server" -ForegroundColor Yellow

        $buildScript = Join-Path $repoRoot 'server\build_windows.ps1'
        if (!(Test-Path $buildScript)) {
            throw ('找不到脚本: {0}' -f $buildScript)
        }

        # 在新窗口中运行编译和启动
        Start-Process powershell.exe -WorkingDirectory $repoRoot -ArgumentList @(
            '-NoExit',
            '-ExecutionPolicy', 'Bypass',
            '-File', $buildScript
        )
    }

    Write-Host "  [INFO] Waiting for backend port (max 60s)" -ForegroundColor Yellow
    if (Wait-Port -HostName 'localhost' -Port $BackendPort -TimeoutSec 60) {
        $backendHost = 'localhost'
        Write-Host ("  [OK] 后端已在 localhost:{0} 监听" -f $BackendPort) -ForegroundColor Green
    } elseif ($wslIp -and (Wait-Port -HostName $wslIp -Port $BackendPort -TimeoutSec 10)) {
        $backendHost = $wslIp
        Write-Host ("  [OK] 后端已在 WSL {0}:{1} 监听" -f $wslIp, $BackendPort) -ForegroundColor Green
    } else {
        Write-Host "  [ERR] Backend port not reachable. Check the Server window logs." -ForegroundColor Red
        Write-Host "    - Ensure CMake and a C++ compiler are installed" -ForegroundColor Red
        Write-Host "    - Or use WSL mode: .\start_realfs_web.ps1 -UseWSL" -ForegroundColor Red
        exit 1
    }
}

Write-Host '[2/4] Check/start Web console ...' -ForegroundColor Cyan
if (Test-PortOpen -HostName 'localhost' -Port $WebPort) {
    Write-Host ("  [OK] Web already running at http://{0}:{1}" -f $WebHost, $WebPort) -ForegroundColor Green
} else {
    # 确保 flask 可用
    try {
        python -c "import flask" | Out-Null
    } catch {
        Write-Host "  [INFO] Installing Flask" -ForegroundColor Yellow
        python -m pip install flask
    }

    $webDir = Join-Path $repoRoot 'server\web'
    Start-Process powershell.exe -WorkingDirectory $webDir -ArgumentList @(
        '-NoExit',
        '-Command',
        ('python web_server.py --host {0} --port {1} --backend-host {2} --backend-port {3}' -f $WebHost, $WebPort, $backendHost, $BackendPort)
    )

    Write-Host "  [INFO] Waiting for Web port (max 15s)" -ForegroundColor Yellow
    if (!(Wait-Port -HostName 'localhost' -Port $WebPort -TimeoutSec 15)) {
        Write-Host "  [ERR] Web failed to start. Check the Web window logs." -ForegroundColor Red
        exit 1
    }

    Write-Host ("  [OK] Web started: http://{0}:{1}" -f $WebHost, $WebPort) -ForegroundColor Green
}

Write-Host '[3/4] Open browser ...' -ForegroundColor Cyan
Start-Process ("http://{0}:{1}" -f $WebHost, $WebPort)

Write-Host '[4/4] Done' -ForegroundColor Green
Write-Host ('- Backend: {0}:{1}' -f $backendHost, $BackendPort) -ForegroundColor Green
Write-Host ('- Web    : http://{0}:{1}' -f $WebHost, $WebPort) -ForegroundColor Green
