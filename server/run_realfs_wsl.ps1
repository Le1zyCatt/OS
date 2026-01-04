# Windows 上用 WSL 跑“真实文件系统”版 C++ server
# 用法：在 PowerShell 运行：
#   cd .\server
#   .\run_realfs_wsl.ps1
#
# 说明：
# - 会在 WSL 中执行 build_linux.sh（编译 filesystem + server，生成磁盘镜像）
# - 然后在 WSL 中启动 server（默认监听 0.0.0.0:8080）

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Convert-ToWslPath([string]$winPath) {
    $p = $winPath -replace '\\','/'
    if ($p -match '^([A-Za-z]):/(.*)$') {
        $drive = $matches[1].ToLower()
        $rest = $matches[2]
        return "/mnt/$drive/$rest"
    }
    return $p
}

$wslRepo = Convert-ToWslPath $repoRoot

# 注意：部分 WSL 发行版可能没有 bash（例如精简环境）。
# 这里使用 POSIX sh；并且把要执行的内容写入 WSL 内的临时脚本后再运行，避免 -lc 参数中引号/括号被破坏。
$scriptTemplate = @'
set -e

REPO="__WSL_REPO__"
FS_DIR="$REPO/filesystem"
SERVER_DIR="$REPO/server"
DISK_IMG="$REPO/filesystem/disk/disk.img"

echo '[WSL] filesystem build...'
cd "$FS_DIR/src"
make

echo '[WSL] ensure disk image...'
mkdir -p "$FS_DIR/disk"
if [ ! -f "$DISK_IMG" ]; then
    "$FS_DIR/bin/mkfs" "$DISK_IMG"
fi

echo '[WSL] server build real filesystem...'
cd "$SERVER_DIR"
rm -rf build_real_fs
mkdir -p build_real_fs
cd build_real_fs
cmake .. -DUSE_REAL_FILESYSTEM=ON

JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)
if [ -z "$JOBS" ]; then
    JOBS=2
fi
make -j"$JOBS"

echo '[WSL] run server...'
./server
'@

$wslScript = $scriptTemplate.Replace('__WSL_REPO__', $wslRepo)

$cmd = @'
TMP="/tmp/os_realfs_server.sh"
cat > "$TMP" <<'EOF'
__SCRIPT_BODY__
EOF
# normalize possible CRLF -> LF (PowerShell here-strings may inject \r)
tr -d '\r' < "$TMP" > "$TMP.tmp" || true
if [ -f "$TMP.tmp" ]; then mv "$TMP.tmp" "$TMP"; fi
chmod +x "$TMP"
sh "$TMP"
'@

$cmd = $cmd.Replace('__SCRIPT_BODY__', $wslScript)

Write-Host "[WSL] repo: $wslRepo" -ForegroundColor Cyan
Write-Host "[WSL] build + run real filesystem server..." -ForegroundColor Cyan
wsl sh -c $cmd
