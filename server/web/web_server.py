#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
论文审稿系统 Web 控制台服务器
Academic Paper Review System - Web Console Server

这个 Flask 服务器作为 Web 前端和 C++ 后端服务器之间的桥梁。
前端通过 HTTP API 发送命令，本服务器将命令转发给 C++ TCP 服务器，
然后将响应返回给前端。

使用方法:
    python web_server.py                    # 默认连接 localhost:8080
    python web_server.py --host 0.0.0.0     # 允许外部访问
    python web_server.py --port 5000        # 指定 Web 服务器端口
    python web_server.py --backend-host 192.168.1.100 --backend-port 8080  # 指定后端
"""

import socket
import argparse
import time
import os
from pathlib import Path
from flask import Flask, render_template, request, jsonify, send_from_directory, Response

# 配置
BACKEND_HOST = 'localhost'
BACKEND_PORT = 8080
SOCKET_TIMEOUT = 10.0
BUFFER_SIZE = 65536

# Flask 应用
app = Flask(__name__,
            template_folder='templates',
            static_folder='static')


def send_to_backend_raw(command: str) -> bytes:
    """发送命令到后端并返回原始 bytes（不做解码）。"""
    try:
        with socket.create_connection((BACKEND_HOST, BACKEND_PORT), timeout=SOCKET_TIMEOUT) as sock:
            sock.settimeout(SOCKET_TIMEOUT)

            sock.sendall(command.encode('utf-8'))

            try:
                sock.shutdown(socket.SHUT_WR)
            except OSError:
                pass

            chunks = []
            while True:
                data = sock.recv(BUFFER_SIZE)
                if not data:
                    break
                chunks.append(data)
            return b''.join(chunks)
    except ConnectionRefusedError:
        return 'ERROR: 无法连接到后端服务器 (连接被拒绝)'.encode('utf-8')
    except socket.timeout:
        return 'ERROR: 后端服务器响应超时'.encode('utf-8')
    except Exception as e:
        return f'ERROR: 通信错误 - {str(e)}'.encode('utf-8', errors='replace')


def send_to_backend(command: str) -> str:
    """
    发送命令到 C++ 后端服务器。
    
    服务器使用 "一条命令一个连接" 的模式，
    所以每次调用都会创建新连接。
    """
    raw = send_to_backend_raw(command)
    return raw.decode('utf-8', errors='replace')


def check_backend_connection() -> bool:
    """检查后端服务器是否可连接。"""
    try:
        with socket.create_connection((BACKEND_HOST, BACKEND_PORT), timeout=3.0):
            return True
    except:
        return False


# ===== 路由 =====

@app.route('/')
def index():
    """主页面"""
    return render_template('index.html')


@app.route('/api/command', methods=['POST'])
def api_command():
    """
    执行命令 API
    
    请求格式:
        POST /api/command
        Content-Type: application/json
        {"command": "LOGIN admin admin123"}
    
    响应格式:
        {"response": "OK: <token> ROLE=ADMIN", "success": true}
    """
    try:
        data = request.get_json()
        command = data.get('command', '').strip()
        
        if not command:
            return jsonify({
                'response': 'ERROR: 命令不能为空',
                'success': False
            })
        
        # 发送命令到后端
        response = send_to_backend(command)
        
        # 判断是否成功
        success = response.startswith('OK')
        
        return jsonify({
            'response': response,
            'success': success
        })
    
    except Exception as e:
        return jsonify({
            'response': f'ERROR: 服务器内部错误 - {str(e)}',
            'success': False
        })


@app.route('/api/command_raw', methods=['POST'])
def api_command_raw():
    """执行命令并返回原始 bytes（用于 READ 二进制内容，例如 PDF/docx/rtf）。"""
    try:
        data = request.get_json()
        command = (data.get('command', '') if isinstance(data, dict) else '').strip()
        if not command:
            payload = 'ERROR: 命令不能为空'.encode('utf-8')
            return Response(payload, mimetype='application/octet-stream', headers={
                'X-Backend-Success': '0',
                'X-Backend-Length': str(len(payload)),
            })

        raw = send_to_backend_raw(command)
        success = raw.startswith(b'OK')
        return Response(raw, mimetype='application/octet-stream', headers={
            'X-Backend-Success': '1' if success else '0',
            'X-Backend-Length': str(len(raw)),
        })
    except Exception as e:
        payload = f'ERROR: 服务器内部错误 - {str(e)}'.encode('utf-8', errors='replace')
        return Response(payload, mimetype='application/octet-stream', headers={
            'X-Backend-Success': '0',
            'X-Backend-Length': str(len(payload)),
        })


@app.route('/api/status')
def api_status():
    """
    获取后端服务器连接状态
    
    响应格式:
        {"connected": true, "host": "localhost", "port": 8080}
    """
    connected = check_backend_connection()
    return jsonify({
        'connected': connected,
        'host': BACKEND_HOST,
        'port': BACKEND_PORT
    })


@app.route('/api/filesystem/info')
def api_filesystem_info():
    """
    获取文件系统基本信息
    
    从后端服务器获取真实的文件系统信息（通过 FS_INFO 命令）。
    """
    try:
        response = send_to_backend("FS_INFO")
        
        if response.startswith("OK:"):
            # 解析响应
            lines = response.split('\n')
            info = {}
            for line in lines[1:]:  # 跳过 "OK:"
                if '=' in line:
                    key, value = line.split('=', 1)
                    info[key.strip()] = value.strip()
            
            block_size = int(info.get('BLOCK_SIZE', 1024))
            block_count = int(info.get('BLOCK_COUNT', 8192))
            free_blocks = int(info.get('FREE_BLOCK_COUNT', 8069))
            data_block_start = int(info.get('DATA_BLOCK_START', 123))
            
            return jsonify({
                'disk_size': f'{block_size * block_count // (1024 * 1024)} MB',
                'block_size': f'{block_size // 1024} KB' if block_size >= 1024 else f'{block_size} B',
                'total_blocks': block_count,
                'data_blocks': block_count - data_block_start,
                'free_blocks': free_blocks,
                'used_blocks': block_count - data_block_start - free_blocks,
                'system_blocks': data_block_start,
                'inode_count': int(info.get('INODE_COUNT', 1024)),
                'free_inodes': int(info.get('FREE_INODE_COUNT', 1023)),
                'data_block_start': data_block_start,
                'snapshot_count': int(info.get('SNAPSHOT_COUNT', 0)),
                'is_real_fs': info.get('FS_TYPE', 'SIMULATED') == 'REAL'
            })
        else:
            # 回退到硬编码的默认值
            return jsonify({
                'disk_size': '8 MB',
                'block_size': '1 KB',
                'total_blocks': 8192,
                'data_blocks': 8069,
                'free_blocks': 8069,
                'used_blocks': 0,
                'system_blocks': 123,
                'inode_count': 1024,
                'free_inodes': 1023,
                'data_block_start': 123,
                'snapshot_count': 0,
                'is_real_fs': False,
                'error': response
            })
    except Exception as e:
        return jsonify({
            'disk_size': '8 MB',
            'block_size': '1 KB', 
            'total_blocks': 8192,
            'data_blocks': 8069,
            'free_blocks': 8069,
            'used_blocks': 0,
            'system_blocks': 123,
            'inode_count': 1024,
            'free_inodes': 1023,
            'data_block_start': 123,
            'snapshot_count': 0,
            'is_real_fs': False,
            'error': str(e)
        })


@app.route('/api/snapshots', methods=['GET'])
def api_list_snapshots():
    """
    获取所有快照列表
    
    需要传递 token 参数进行认证
    """
    token = request.args.get('token', '')
    if not token:
        return jsonify({
            'success': False,
            'error': '需要登录才能查看快照列表',
            'snapshots': []
        })
    
    try:
        response = send_to_backend(f"BACKUP_LIST {token}")
        if response.startswith("OK:"):
            # 解析快照列表
            content = response[3:].strip()
            # 处理 "(no snapshots)" 特殊标记
            if content == "(no snapshots)" or not content:
                snapshots = []
            else:
                parts = content.split()
                # 过滤掉无效的快照名（如 "/" 或空字符串）
                snapshots = [name for name in parts if name and name != "/" and not name.startswith("(")]
            return jsonify({
                'success': True,
                'snapshots': snapshots
            })
        else:
            return jsonify({
                'success': False,
                'error': response,
                'snapshots': []
            })
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e),
            'snapshots': []
        })


@app.route('/api/snapshots/create', methods=['POST'])
def api_create_snapshot():
    """
    创建新快照
    
    请求格式:
        POST /api/snapshots/create
        Content-Type: application/json
        {"token": "session_token", "name": "snapshot_name"}
    """
    try:
        data = request.get_json()
        token = data.get('token', '')
        name = data.get('name', '')
        
        if not token:
            return jsonify({
                'success': False,
                'error': '需要登录才能创建快照'
            })
        
        if not name:
            return jsonify({
                'success': False,
                'error': '快照名称不能为空'
            })
        
        # 使用 BACKUP_CREATE 命令
        response = send_to_backend(f"BACKUP_CREATE {token} / {name}")
        success = response.startswith("OK")
        
        return jsonify({
            'success': success,
            'response': response
        })
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        })


@app.route('/api/snapshots/restore', methods=['POST'])
def api_restore_snapshot():
    """
    恢复快照
    
    请求格式:
        POST /api/snapshots/restore
        Content-Type: application/json
        {"token": "session_token", "name": "snapshot_name"}
    """
    try:
        data = request.get_json()
        token = data.get('token', '')
        name = data.get('name', '')
        
        if not token:
            return jsonify({
                'success': False,
                'error': '需要登录才能恢复快照'
            })
        
        if not name:
            return jsonify({
                'success': False,
                'error': '快照名称不能为空'
            })
        
        # 使用 BACKUP_RESTORE 命令
        response = send_to_backend(f"BACKUP_RESTORE {token} {name}")
        success = response.startswith("OK")
        
        return jsonify({
            'success': success,
            'response': response
        })
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        })


@app.route('/static/<path:filename>')
def serve_static(filename):
    """提供静态文件"""
    return send_from_directory(app.static_folder, filename)


# ===== 辅助函数 =====

def print_banner():
    """打印启动横幅"""
    banner = """
╭───────────────────────────────────────────────────────────────╮
│       学术论文审稿系统 - Web 控制台服务器                        │
│       Academic Paper Review System - Web Console Server       │
╰───────────────────────────────────────────────────────────────╯
"""
    print(banner)


def main():
    parser = argparse.ArgumentParser(
        description='论文审稿系统 Web 控制台服务器',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
    python web_server.py                             # 使用默认配置
    python web_server.py --host 0.0.0.0 --port 5000  # 允许外部访问
    python web_server.py --backend-port 9000         # 连接不同端口的后端
        """
    )
    
    parser.add_argument('--host', default='127.0.0.1',
                        help='Web 服务器监听地址 (默认: 127.0.0.1)')
    parser.add_argument('--port', type=int, default=5000,
                        help='Web 服务器监听端口 (默认: 5000)')
    parser.add_argument('--backend-host', default='localhost',
                        help='C++ 后端服务器地址 (默认: localhost)')
    parser.add_argument('--backend-port', type=int, default=8080,
                        help='C++ 后端服务器端口 (默认: 8080)')
    parser.add_argument('--debug', action='store_true',
                        help='启用调试模式')
    
    args = parser.parse_args()
    
    # 更新后端配置
    global BACKEND_HOST, BACKEND_PORT
    BACKEND_HOST = args.backend_host
    BACKEND_PORT = args.backend_port
    
    print_banner()
    
    # 检查后端连接
    print(f"📡 后端服务器: {BACKEND_HOST}:{BACKEND_PORT}")
    if check_backend_connection():
        print("   ✓ 后端服务器连接成功")
    else:
        print("   ⚠ 警告: 无法连接到后端服务器")
        print("      请确保 C++ 服务器已启动并监听正确的端口")
    
    print()
    print(f"🌐 Web 控制台: http://{args.host}:{args.port}")
    print(f"   在浏览器中打开上述地址以使用控制台")
    print()
    print("按 Ctrl+C 停止服务器")
    print("-" * 60)
    
    # 启动 Flask
    app.run(
        host=args.host,
        port=args.port,
        debug=args.debug,
        threaded=True
    )


if __name__ == '__main__':
    main()
