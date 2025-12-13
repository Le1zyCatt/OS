import socket
import time

HOST = 'localhost'  # 服务器地址
PORT = 8080         # 服务器端口
BUFFER_SIZE = 4096  # 增大缓冲区避免截断长响应

SESSION_TOKEN = None

def send_command(command):
    """发送单条指令并获取响应（自动重连）"""
    max_retries = 3
    for attempt in range(max_retries):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.settimeout(5)  # 5秒超时防止卡死
                s.connect((HOST, PORT))
                s.sendall(command.encode('utf-8'))
                
                # 接收完整响应（处理分包问题）
                response = b""
                while True:
                    chunk = s.recv(BUFFER_SIZE)
                    if not chunk:
                        break
                    response += chunk
                    if len(chunk) < BUFFER_SIZE:  # 可能收到完整数据
                        break
                
                return response.decode('utf-8', errors='replace')
                
        except (socket.timeout, ConnectionRefusedError) as e:
            if attempt < max_retries - 1:
                print(f"⚠️ 连接失败，{2-attempt}秒后重试... ({str(e)})")
                time.sleep(2 - attempt)
            else:
                return f"❌ 永久连接失败: {str(e)}"
        except Exception as e:
            return f"❌ 未知错误: {str(e)}"

if __name__ == "__main__":
    print("="*50)
    print(f"📡 已连接到 {HOST}:{PORT} | 输入 'exit' 退出")
    print("="*50)
    
    while True:
        try:
            # 获取用户输入（支持中文）
            cmd = input("\n➡️ 请输入指令 (exit退出): ").strip()
            
            if cmd.lower() == 'exit':
                print("👋 客户端已退出")
                break
                
            if not cmd:
                print("⚠️  指令不能为空！")
                continue

            # 自动注入 session token（除 LOGIN 外）
            upper = cmd.strip().split(" ", 1)[0].upper() if cmd.strip() else ""
            if upper not in {"LOGIN"}:
                if SESSION_TOKEN is None:
                    print("⚠️  未登录：请先执行 LOGIN <user> <pass>")
                    continue

                # 如果用户已经手动带了 token，就不重复注入
                parts = cmd.split()
                if len(parts) >= 2 and parts[1] == SESSION_TOKEN:
                    pass
                else:
                    cmd = f"{upper} {SESSION_TOKEN} " + cmd[len(parts[0]):].lstrip()
                
            # 发送指令并显示结果
            print("\n⏳ 等待服务器响应...")
            response = send_command(cmd)

            # 解析 LOGIN 返回的 token
            if upper == "LOGIN" and response.startswith("OK:"):
                # 期望格式：OK: <token> ROLE=...
                try:
                    token_part = response.split("OK:", 1)[1].strip().split()[0]
                    if token_part:
                        SESSION_TOKEN = token_part
                        print(f"[i] 当前会话 token: {SESSION_TOKEN}")
                except Exception:
                    pass
            
            # 彩色化输出响应
            if response.startswith("❌"):
                print(f"\033[91m{response}\033[0m")  # 红色错误
            elif "成功" in response or "OK" in response.upper():
                print(f"\033[92m✅ 服务器响应:\n{response}\033[0m")  # 绿色成功
            else:
                print(f"\033[94mℹ️  服务器响应:\n{response}\033[0m")  # 蓝色普通响应
                
        except KeyboardInterrupt:
            print("\n\n✋ 检测到 Ctrl+C，正在安全退出...")
            break
        except Exception as e:
            print(f"\033[91m❌ 未处理异常: {str(e)}\033[0m")

    print("="*50)