# 论文审稿系统 Web 控制台

这是一个基于 Web 的可视化控制台，用于与论文审稿系统进行交互。

## 功能特性

- **类 CLI 终端界面**: 提供熟悉的命令行交互体验
- **用户身份切换**: 快速切换不同角色（管理员/编辑/审稿人/作者）
- **文件系统视图**: 可视化展示磁盘使用情况、块分配、缓存统计
- **快照管理**: 创建、查看、恢复文件系统快照（支持 COW 机制）
- **实时状态**: 显示服务器连接状态和当前路径
- **一键测试**: 运行完整的冒烟测试，覆盖所有命令
- **命令面板**: 右侧可呼出命令列表，点击复制到剪贴板

## 目录结构

```
web/
├── web_server.py          # Flask Web 服务器
├── README.md              # 本文档
├── templates/
│   └── index.html         # 主页面模板
└── static/
    ├── css/
    │   └── style.css      # 样式文件
    └── js/
        └── app.js         # 前端 JavaScript
```

## 快速开始

### Windows + WSL：一键启动（真实文件系统 + Web）

在仓库根目录打开 PowerShell：

```powershell
powershell -ExecutionPolicy Bypass -File .\start_realfs_web.ps1
```

脚本会自动：
- 检查 `8080` 后端是否已启动（WSL 真实文件系统版），未启动则启动
- 检查 `5000` Web 是否已启动，未启动则启动
- 打开浏览器 `http://127.0.0.1:5000`

### 1. 安装依赖

```bash
pip install flask
```

### 2. 启动 C++ 后端服务器

#### Windows (使用内存文件系统)

确保 C++ 服务器已编译并运行在 `localhost:8080`：

```bash
cd server/build/Release
./server.exe
```

#### Linux (启用真实文件系统)

使用提供的构建脚本启用真实文件系统：

```bash
cd server
chmod +x build_linux.sh
./build_linux.sh

# 启动服务器
cd build_real_fs
./server
```

### 3. 启动 Web 服务器

```bash
cd server/web
python web_server.py
```

### 4. 访问控制台

在浏览器中打开: http://127.0.0.1:5000

## 命令行参数

```bash
python web_server.py [选项]

选项:
    --host HOST           Web 服务器监听地址 (默认: 127.0.0.1)
    --port PORT           Web 服务器监听端口 (默认: 5000)
    --backend-host HOST   C++ 后端服务器地址 (默认: localhost)
    --backend-port PORT   C++ 后端服务器端口 (默认: 8080)
    --debug               启用调试模式
```

### 示例

```bash
# 允许局域网访问
python web_server.py --host 0.0.0.0

# 连接远程后端
python web_server.py --backend-host 192.168.1.100 --backend-port 9000

# 开发模式（自动重载）
python web_server.py --debug
```

## 使用指南

### 登录

1. 点击左侧用户卡片快速登录（使用预设账号密码）
2. 或在终端输入: `LOGIN <用户名> <密码>`
3. 同一时间只能有一个用户处于登录状态

预设用户:
- `admin / admin123` - 管理员
- `editor / editor123` - 编辑
- `reviewer / reviewer123` - 审稿人
- `author / author123` - 作者

### 一键测试

点击左下角的 **🚀 一键测试所有功能** 按钮，可运行完整的冒烟测试。

测试内容包括:
- ADMIN: PWD, MKDIR, WRITE, READ, CD, LS, TREE, USER_LIST, SYSTEM_STATUS, CACHE_STATS
- 快照: BACKUP_CREATE, BACKUP_LIST, BACKUP_RESTORE
- AUTHOR: PAPER_UPLOAD, STATUS, REVIEWS_DOWNLOAD
- EDITOR: ASSIGN_REVIEWER, DECIDE
- REVIEWER: PAPER_DOWNLOAD, REVIEW_SUBMIT

测试会自动按角色切换并显示每条命令的输入和输出（类似 `test_client.py --smoke`）。

### 命令面板

点击右上角的 **📋 命令** 按钮呼出命令面板，点击任意命令可直接复制到剪贴板。

### 视图切换

点击左侧"视图"区域的标签切换:
- **命令终端**: CLI 交互界面
- **文件系统**: 磁盘使用情况、缓存统计、目录树
- **快照管理**: 查看和管理快照

### 常用命令

```
# 文件操作
LS [path]              列出目录
PWD                    显示当前路径
CD <path>              切换目录
MKDIR <path>           创建目录
READ <path>            读取文件
WRITE <path> <content> 写入文件

# 论文流程
PAPER_UPLOAD <id> <content>   上传论文（作者）
ASSIGN_REVIEWER <id> <user>   分配审稿人（编辑）
PAPER_DOWNLOAD <id>           下载论文（审稿人）
REVIEW_SUBMIT <id> <review>   提交评审（审稿人）
DECIDE <id> ACCEPT|REJECT     做出决定（编辑）

# 系统管理
USER_LIST              用户列表
SYSTEM_STATUS          系统状态
CACHE_STATS            缓存统计
BACKUP_CREATE / <name> 创建快照
BACKUP_LIST            快照列表
BACKUP_RESTORE <name>  恢复快照
```

## 技术说明

### 架构

```
┌─────────────────┐      HTTP       ┌──────────────────┐      TCP       ┌─────────────────┐
│   Web Browser   │ ◄──────────────► │  Flask Server    │ ◄──────────────► │  C++ Server     │
│   (前端界面)     │     :5000       │  (web_server.py) │     :8080       │  (main.cpp)     │
└─────────────────┘                 └──────────────────┘                 └─────────────────┘
```

### API 端点

| 端点 | 方法 | 说明 |
|------|------|------|
| `/` | GET | 主页面 |
| `/api/command` | POST | 执行命令 |
| `/api/command_raw` | POST | 执行命令（返回原始 bytes，用于 READ PDF/docx/rtf 等二进制） |
| `/api/status` | GET | 获取连接状态 |
| `/api/filesystem/info` | GET | 获取文件系统信息 |

### 请求示例

```javascript
// 执行命令
fetch('/api/command', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ command: 'LOGIN admin admin123' })
})
.then(r => r.json())
.then(data => console.log(data.response));
```

## 常见问题

### Q: 无法连接到后端服务器

确保:
1. C++ 服务器已启动
2. 端口号正确（默认 8080）
3. 防火墙未阻止连接

### Q: 页面显示但命令无响应

检查:
1. Web 服务器控制台是否有错误信息
2. 后端服务器是否仍在运行
3. 使用 `test_client.py` 测试后端是否正常

### Q: 样式显示异常

尝试:
1. 清除浏览器缓存
2. 检查 static 文件夹结构是否正确
3. 确保 Flask 正确服务静态文件

## 许可证

本项目为 SCUT 操作系统课程大作业的一部分。
