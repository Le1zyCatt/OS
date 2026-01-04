#!/bin/bash
# ============================================================
# Linux 构建脚本 - 启用真实文件系统
# ============================================================
# 此脚本用于在 Linux 环境下编译服务器并启用真实文件系统模块
#
# 使用方法:
#   chmod +x build_linux.sh
#   ./build_linux.sh
#
# 依赖:
#   - g++ (支持 C++17)
#   - cmake (>= 3.10)
#   - make
# ============================================================

set -e  # 遇到错误时退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}============================================================${NC}"
echo -e "${BLUE}    学术论文审稿系统 - Linux 构建脚本 (启用真实文件系统)${NC}"
echo -e "${BLUE}============================================================${NC}"
echo ""

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SERVER_DIR="$SCRIPT_DIR"
FS_DIR="$PROJECT_ROOT/filesystem"

echo -e "${YELLOW}[1/5]${NC} 检查目录结构..."
echo "  - 项目根目录: $PROJECT_ROOT"
echo "  - 服务器目录: $SERVER_DIR"
echo "  - 文件系统目录: $FS_DIR"

if [ ! -d "$FS_DIR" ]; then
    echo -e "${RED}错误: 找不到 filesystem 目录${NC}"
    exit 1
fi

# 检查编译器
echo -e "${YELLOW}[2/5]${NC} 检查编译环境..."
if ! command -v g++ &> /dev/null; then
    echo -e "${RED}错误: 未找到 g++${NC}"
    exit 1
fi

if ! command -v cmake &> /dev/null; then
    echo -e "${RED}错误: 未找到 cmake${NC}"
    exit 1
fi

echo -e "  - g++: $(g++ --version | head -n1)"
echo -e "  - cmake: $(cmake --version | head -n1)"

# 构建 filesystem 模块
echo ""
echo -e "${YELLOW}[3/5]${NC} 构建 filesystem 模块..."
cd "$FS_DIR/src"
make clean 2>/dev/null || true
make all

# 创建磁盘镜像
echo ""
echo -e "${YELLOW}[4/5]${NC} 初始化磁盘镜像..."
DISK_IMG="$FS_DIR/disk/disk.img"
if [ ! -f "$DISK_IMG" ]; then
    echo "  创建新的磁盘镜像: $DISK_IMG"
    mkdir -p "$FS_DIR/disk"
    "$FS_DIR/bin/mkfs" "$DISK_IMG"
    echo -e "  ${GREEN}✓ 磁盘镜像创建成功${NC}"
else
    echo -e "  ${GREEN}✓ 磁盘镜像已存在${NC}"
fi

# 构建服务器
echo ""
echo -e "${YELLOW}[5/5]${NC} 构建服务器 (启用真实文件系统)..."
cd "$SERVER_DIR"
rm -rf build_real_fs
mkdir -p build_real_fs
cd build_real_fs

cmake .. -DUSE_REAL_FILESYSTEM=ON
make -j$(nproc)

echo ""
echo -e "${GREEN}============================================================${NC}"
echo -e "${GREEN}    ✅ 构建成功!${NC}"
echo -e "${GREEN}============================================================${NC}"
echo ""
echo -e "服务器可执行文件: ${BLUE}$SERVER_DIR/build_real_fs/server${NC}"
echo -e "磁盘镜像位置: ${BLUE}$DISK_IMG${NC}"
echo ""
echo -e "启动服务器:"
echo -e "  ${YELLOW}cd $SERVER_DIR/build_real_fs${NC}"
echo -e "  ${YELLOW}./server${NC}"
echo ""
echo -e "启动 Web 控制台:"
echo -e "  ${YELLOW}cd $SERVER_DIR/web${NC}"
echo -e "  ${YELLOW}pip install flask${NC}"
echo -e "  ${YELLOW}python web_server.py${NC}"
echo ""
echo -e "然后访问 ${BLUE}http://localhost:5000${NC} 打开 Web 控制台"
echo ""
