"""server/test/test_client.py

用途：
  这是一个“手动测试客户端”，用于在命令行里逐条发送 server 的 CLI 指令，查看返回结果。

特点：
  1) 全中文交互提示 + 详细中文注释，便于当作测试说明书使用
  2) 自动处理：除 LOGIN 外，其他命令会自动注入当前会话 token
  3) 支持多会话（多账号）切换：可同时登录 admin/editor/reviewer/author/guest，随时切换活跃账号
  4) 内置：
     - “可用指令总览”（本地打印，不依赖 server）
     - “手动测试用例清单”（按角色/场景给出建议命令序列）

前置条件：
  - server 已启动并监听 host:port（默认 localhost:8080）
  - 本脚本仅做 client 侧辅助，不修改 server 状态机逻辑
"""

from __future__ import annotations

import argparse
import socket
import time
from dataclasses import dataclass
from typing import Dict, Optional


# 单次 recv 的读取上限。
# 由于 server 可能返回多行文本（例如 STATUS/USER_LIST），所以需要循环 recv 直到对端关闭。
BUFFER_SIZE = 4096


@dataclass
class ClientConfig:
    """客户端连接配置。"""

    host: str = "localhost"
    port: int = 8080
    timeout_sec: float = 5.0
    retries: int = 3


class ServerClient:
    """一个最小的 TCP 文本协议客户端。

    注意：server 的实现是“一条命令一个连接”（见 server/main.cpp），
    所以这里每次 send 都会创建一次连接，发送命令后等待 server 关闭连接。
    """

    def __init__(self, cfg: ClientConfig):
        self.cfg = cfg

        # 支持多账号：alias -> token
        # 例如：sessions["admin"] = "<token>"
        self.sessions: Dict[str, str] = {}
        self.active_alias: Optional[str] = None

    # ----------------------------
    # 网络层：发送/接收
    # ----------------------------
    def send_raw(self, command: str) -> str:
        """发送原始命令（不做 token 注入）。"""

        command = command.strip()
        if not command:
            return "ERROR: 指令不能为空"

        last_exc: Optional[Exception] = None

        for attempt in range(self.cfg.retries):
            try:
                with socket.create_connection((self.cfg.host, self.cfg.port), timeout=self.cfg.timeout_sec) as s:
                    s.settimeout(self.cfg.timeout_sec)

                    # 发送 UTF-8 文本
                    s.sendall(command.encode("utf-8"))

                    # 告诉 server “我发完了”，便于对端尽快返回并关闭
                    try:
                        s.shutdown(socket.SHUT_WR)
                    except OSError:
                        pass

                    # 循环接收，直到 server 关闭连接
                    chunks: list[bytes] = []
                    while True:
                        data = s.recv(BUFFER_SIZE)
                        if not data:
                            break
                        chunks.append(data)

                return b"".join(chunks).decode("utf-8", errors="replace")
            except (ConnectionRefusedError, socket.timeout) as e:
                last_exc = e
                # 简单重试：短暂 sleep（递增）
                if attempt < self.cfg.retries - 1:
                    time.sleep(0.2 * (attempt + 1))
                    continue
            except Exception as e:
                last_exc = e
                break

        return f"ERROR: 连接/发送失败：{last_exc}"

    def _get_active_token(self) -> Optional[str]:
        if not self.active_alias:
            return None
        return self.sessions.get(self.active_alias)

    def send(self, command: str, *, auto_token: bool = True) -> str:
        """发送命令（默认自动注入 token）。

        规则：
          - LOGIN 不注入 token
          - 其他命令：若用户没手动写 token，则自动插入为第二个参数
        """

        command = command.strip()
        if not command:
            return "ERROR: 指令不能为空"

        cmd = command.split(" ", 1)[0].upper()
        if not auto_token or cmd == "LOGIN":
            return self.send_raw(command)

        token = self._get_active_token()
        if not token:
            return "ERROR: 未登录（没有可用 token）。请先执行：LOGIN <用户名> <密码>"

        parts = command.split()

        # 用户已经手动带了 token（第二段就是 token）则不重复注入
        if len(parts) >= 2 and parts[1] == token:
            return self.send_raw(command)

        # 否则注入 token 到第二段
        rest = command[len(parts[0]) :].lstrip()
        injected = f"{cmd} {token} {rest}" if rest else f"{cmd} {token}"
        return self.send_raw(injected)

    # ----------------------------
    # 会话管理：登录/登出/切换
    # ----------------------------
    def login(self, username: str, password: str, *, alias: Optional[str] = None) -> str:
        """登录并保存 token。

        server 返回格式：OK: <token> ROLE=...
        """

        resp = self.send_raw(f"LOGIN {username} {password}")
        if not resp.startswith("OK:"):
            return resp

        # 解析 token
        token = None
        try:
            token = resp.split("OK:", 1)[1].strip().split()[0]
        except Exception:
            token = None

        if not token:
            return "ERROR: 登录成功但无法解析 token（响应格式异常）"

        key = alias or username
        self.sessions[key] = token
        self.active_alias = key
        return resp + f"\n[i] 已保存会话：{key}"

    def logout(self, alias: Optional[str] = None) -> str:
        """登出并清理本地 token 记录。"""

        key = alias or self.active_alias
        if not key:
            return "ERROR: 当前没有活跃会话可登出"

        token = self.sessions.get(key)
        if not token:
            return f"ERROR: 会话不存在：{key}"

        resp = self.send_raw(f"LOGOUT {token}")
        if resp.startswith("OK:"):
            self.sessions.pop(key, None)
            if self.active_alias == key:
                self.active_alias = None
        return resp

    def use(self, alias: str) -> str:
        if alias not in self.sessions:
            return f"ERROR: 未找到会话：{alias}（请先 LOGIN 或用 :sessions 查看）"
        self.active_alias = alias
        return f"OK: 已切换到会话 {alias}"


# ----------------------------
# 本地帮助：指令总览 + 手动测试清单
# ----------------------------

def print_local_help() -> None:
        """统一帮助。

        设计目标：
            - 你输入 :help 与输入 HELP/help，看到的内容完全一致
            - 提示语全部中文；命令关键字（LOGIN/READ/...）保留英文大写，便于与 server 匹配
        """

        print(
                """\
==================== 帮助 ====================

[如何使用]
1) 先登录：LOGIN <用户名> <密码>
     - 登录成功后，客户端会自动保存 token，并设为“当前活跃会话”。


[本地指令（不会发给 server，以冒号 : 开头）]
    :help              显示本帮助（与 HELP/help 完全一致）
    :commands          显示 server 全部可用指令总览（按角色分类）
    :cases             显示“手动测试用例清单”（建议命令序列，可复制粘贴）
    :sessions          显示当前保存的会话列表
    :use <alias>       切换活跃会话（alias 默认是用户名；也可 LOGIN 第三个参数自定义别名）
    :logout [alias]    登出（默认登出活跃会话）
    :serverhelp        获取并显示 server 实际返回的 HELP（会做中文化展示；需要已登录）
    :exit              退出

===============================================================
"""
    )


def _translate_server_help_to_chinese(resp: str) -> str:
    """将 server 返回的 HELP 文本做最小中文化。

    注意：
      - 命令关键字保持原样（LOGIN/READ/...）
      - 仅把提示语/标签翻译成中文，保证“全部中文输出”的体验
    """

    if not resp:
        return "(空响应)"

    if resp.startswith("ERROR:"):
        return "错误：" + resp[len("ERROR:") :].lstrip()

    if not resp.startswith("OK:"):
        return resp

    text = resp
    text = text.replace("OK:", "成功：")
    text = text.replace("ROLE=", "角色=")
    text = text.replace("Commands:", "可用命令：")
    text = text.replace("Common:", "通用：")
    text = text.replace("Author:", "作者：")
    text = text.replace("Reviewer:", "审稿人：")
    text = text.replace("Editor:", "编辑：")
    text = text.replace("Admin:", "管理员：")
    return text


def print_all_server_commands() -> None:
    """打印 server 侧支持的所有命令（来自 server/readme.md 与 CLIProtocol 约定）。"""

    print(
        """\
======== server 可用指令总览 ========

通用：
  LOGIN <user> <pass>
  LOGOUT <token>
  HELP <token>

文件（通用调试）：
  READ <token> <path>
  WRITE <token> <path> <content...>
  MKDIR <token> <path>

作者（AUTHOR）：
  PAPER_UPLOAD <token> <paperId> <content...>
  PAPER_REVISE <token> <paperId> <content...>
  STATUS <token> <paperId>
  REVIEWS_DOWNLOAD <token> <paperId>

审稿人（REVIEWER）：
  PAPER_DOWNLOAD <token> <paperId>
  STATUS <token> <paperId>
  REVIEW_SUBMIT <token> <paperId> <reviewContent...>

编辑（EDITOR）：
  ASSIGN_REVIEWER <token> <paperId> <reviewerUsername>
  DECIDE <token> <paperId> <ACCEPT|REJECT>
  STATUS <token> <paperId>
  REVIEWS_DOWNLOAD <token> <paperId>

管理员（ADMIN）：
  USER_ADD <token> <username> <password> <ADMIN|EDITOR|REVIEWER|AUTHOR|GUEST>
  USER_DEL <token> <username>
  USER_LIST <token>
  BACKUP_CREATE <token> <path> [name]
  BACKUP_LIST <token>
  BACKUP_RESTORE <token> <name>
  SYSTEM_STATUS <token>
    CACHE_STATS <token>
    CACHE_CLEAR <token>

审核（ReviewFlow，当前实现为“提交审核请求”）：
  SUBMIT_REVIEW <token> <operation> <path>

====================================
"""
    )


def print_manual_test_cases() -> None:
    """打印“手动测试用例清单”。

    说明：
      - 这是“建议命令序列”，你可以复制粘贴到本客户端逐条执行。
      - paperId / snapshotName 等建议带时间戳避免冲突。
    """

    print(
        """\
======== 手动测试用例清单（建议）========

[用例 0] 基础连通性
  - 输入一个不存在的命令，应该返回 Unknown command
    NO_SUCH_CMD

[用例 1] 管理员（admin）基本能力：用户管理 + 系统状态
  LOGIN admin admin123
  HELP
  USER_LIST
  USER_ADD test_u1 p123 GUEST
  USER_LIST
  USER_DEL test_u1
  USER_LIST
  SYSTEM_STATUS

[用例 1.1] LRU 缓存（命中/未命中/淘汰）——建议 admin 执行
    说明：CACHE_STATS / CACHE_CLEAR 仅用于测试观测，要求管理员权限。
    (1) 查看初始统计
        CACHE_STATS
    (2) 清空缓存（确保统计从干净状态开始）
        CACHE_CLEAR
        CACHE_STATS
    (3) 命中测试：重复读取同一个文件
        MKDIR /lru
        WRITE /lru/a hello
        CACHE_CLEAR
        READ /lru/a          # 第一次：miss
        READ /lru/a          # 第二次：hit
        CACHE_STATS
    (4) 淘汰测试：读超过 capacity 的不同 key（默认 capacity=64）
        # 先准备文件（写入后会写入缓存，所以要再清空一次）
        WRITE /lru/f01 v01
        WRITE /lru/f02 v02
        ...
        WRITE /lru/f70 v70
        CACHE_CLEAR
        # 依次读取 f01~f70（此时 miss 会快速增长，cache size 最终应为 64）
        READ /lru/f01
        READ /lru/f02
        ...
        READ /lru/f70
        CACHE_STATS
        # 再读一次 f01（大概率已被淘汰）与 f70（大概率仍在缓存）
        READ /lru/f01
        READ /lru/f70
        CACHE_STATS

[用例 2] 文件读写（通用调试）
  MKDIR /tmp
  WRITE /tmp/a hello
  READ /tmp/a

[用例 3] 备份/恢复（建议先用 admin）
  BACKUP_CREATE / snap_123
  BACKUP_LIST
  WRITE /tmp/a changed
  READ /tmp/a
  BACKUP_RESTORE snap_123
  READ /tmp/a

[用例 4] 提交审核请求（ReviewFlow）
  SUBMIT_REVIEW DELETE /tmp/a

[用例 5] Guest 权限验证（读允许/写拒绝）
  LOGIN guest guest
  READ /tmp/a
  WRITE /tmp/a should_fail

[用例 6] 论文全流程（建议多账号切换执行）
  (1) 作者上传
    LOGIN author author123
    PAPER_UPLOAD p1001 content_v1
    STATUS p1001

  (2) 编辑分配审稿人
    LOGIN editor editor123
    ASSIGN_REVIEWER p1001 reviewer

  (3) 审稿人下载/提交评审
    LOGIN reviewer reviewer123
    PAPER_DOWNLOAD p1001
    REVIEW_SUBMIT p1001 looks_good
    STATUS p1001

  (4) 作者/编辑下载评审
    (author) REVIEWS_DOWNLOAD p1001
    (editor) REVIEWS_DOWNLOAD p1001

  (5) 编辑最终决定
    (editor) DECIDE p1001 ACCEPT
    (editor) STATUS p1001

========================================
"""
    )


def run_interactive_shell(client: ServerClient) -> int:
    """交互式手动测试入口。"""

    print("=" * 60)
    print(f"手动测试客户端已启动：{client.cfg.host}:{client.cfg.port}")
    print("输入 :help 查看本地帮助；输入 :exit 退出")
    print("=" * 60)

    while True:
        try:
            prompt = client.active_alias or "(未登录)"
            line = input(f"[{prompt}]> ").strip()
            if not line:
                continue

            # 处理本地指令（以 : 开头）
            if line.startswith(":"):
                parts = line[1:].strip().split()
                cmd = parts[0].lower() if parts else ""
                args = parts[1:]

                if cmd in {"exit", "quit"}:
                    return 0
                if cmd in {"help", "h"}:
                    print_local_help()
                    continue
                if cmd in {"commands", "cmds"}:
                    print_all_server_commands()
                    continue
                if cmd in {"cases", "case"}:
                    print_manual_test_cases()
                    continue
                if cmd in {"sessions", "sess"}:
                    if not client.sessions:
                        print("(空) 当前没有保存任何会话。")
                    else:
                        print("已保存会话：")
                        for k in sorted(client.sessions.keys()):
                            flag = "*" if k == client.active_alias else " "
                            print(f"  {flag} {k}")
                    continue
                if cmd == "use":
                    if len(args) != 1:
                        print("用法：:use <alias>")
                        continue
                    print(client.use(args[0]))
                    continue
                if cmd == "logout":
                    alias = args[0] if args else None
                    print(client.logout(alias))
                    continue
                if cmd == "serverhelp":
                    # 调用 server 的 HELP（需要 token），并把输出做中文化
                    token = client._get_active_token()
                    if not token:
                        print("ERROR: 未登录，无法调用 server 的 HELP。请先 LOGIN。")
                        continue
                    raw = client.send_raw(f"HELP {token}")
                    print(_translate_server_help_to_chinese(raw))
                    continue

                print("ERROR: 未知本地指令。输入 :help 查看可用本地指令。")
                continue

            # 普通情况：当作 server 指令发送
            upper = line.split(" ", 1)[0].upper()

            # 统一：HELP/help 直接显示本地“统一帮助”（不发给 server）
            if upper == "HELP":
                print_local_help()
                continue

            # 1) LOGIN：走专门逻辑，保存 token 并设置活跃会话
            if upper == "LOGIN":
                parts = line.split()
                if len(parts) < 3:
                    print("ERROR: 用法：LOGIN <user> <pass>")
                    continue
                username, password = parts[1], parts[2]
                alias = parts[3] if len(parts) >= 4 else None
                print(client.login(username, password, alias=alias))
                continue

            # 2) LOGOUT：推荐用本地 :logout，但也兼容直接发 server LOGOUT
            if upper == "LOGOUT":
                print(client.send_raw(line))
                continue

            # 3) 其他命令：默认自动注入 token
            resp = client.send(line)
            print(resp)
        except EOFError:
            # 当脚本在没有交互输入的环境下运行（stdin 关闭）会抛 EOFError。
            # 这里做“友好退出”，避免返回码为 1。
            print("\n检测到输入流结束（EOF），已退出。")
            return 0
        except KeyboardInterrupt:
            print("\n检测到 Ctrl+C，已退出。")
            return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="server 手动测试客户端（全中文）")
    parser.add_argument("--host", default="localhost", help="server 地址")
    parser.add_argument("--port", type=int, default=8080, help="server 端口")
    parser.add_argument("--timeout", type=float, default=5.0, help="socket 超时（秒）")
    parser.add_argument("--retries", type=int, default=3, help="连接失败重试次数")
    args = parser.parse_args()

    cfg = ClientConfig(host=args.host, port=args.port, timeout_sec=args.timeout, retries=args.retries)
    client = ServerClient(cfg)
    return run_interactive_shell(client)


if __name__ == "__main__":
    raise SystemExit(main())