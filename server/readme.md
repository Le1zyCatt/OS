server/                     # Server 模块根目录
├── CMakeLists.txt          # 构建配置（可选，用于编译）
├── main.cpp                # 程序入口：初始化服务、启动 TCP 监听
│
├── include/                # 头文件目录
│   ├── auth/               # 身份认证与权限模块（核心：身份确认 + 权限确认）
│   │   ├── Authenticator.h     # 身份认证接口：登录/登出、会话生成
│   │   ├── PermissionChecker.h # 权限检查接口：验证用户操作权限
│   │   └── RolePolicy.h        # 权限策略定义（如角色-权限映射表）
│   │
│   ├── protocol/           # 协议处理模块（核心：调用 FileSystem API 的桥梁）
│   │   ├── CLIProtocol.h       # CLI 通信协议：解析客户端命令（如 "backup create"）
│   │   ├── FSProtocol.h        # FileSystem 通信协议：封装 RPC 调用（如调用 create_snapshot）
│   │   └── ProtocolFactory.h   # 协议工厂：根据请求类型选择协议处理器
│   │
│   ├── business/           # 业务流程模块（轻量级，仅编排关键流程）
│   │   ├── BackupFlow.h        # 备份快照流程：权限检查 → 调用 FS API → 记录日志
│   │   └── ReviewFlow.h        # 审稿流程：触发审核 → 等待审批 → 执行操作
│   │
│   └── cache/              # Server 端缓存（LRU 实现，可选但架构图要求）
│       └── LRUCache.h          # LRU 缓存接口：缓存文件内容，与 FS 通信同步
│
└── src/                    # 源文件目录
    ├── auth/
    │   ├── Authenticator.cpp
    │   ├── PermissionChecker.cpp
    │   └── RolePolicy.cpp
    ├── protocol/
    │   ├── CLIProtocol.cpp
    │   ├── FSProtocol.cpp
    │   └── ProtocolFactory.cpp
    ├── business/
    │   ├── BackupFlow.cpp
    │   └── ReviewFlow.cpp
    └── cache/
        └── LRUCache.cpp

测试的client代码是test里那个py文件

目前用户权限和fs接口还没写好。

fs接口在os\server\src\protocol\FSProtocol.cpp里改

用户权限在auth那几个程序里实现