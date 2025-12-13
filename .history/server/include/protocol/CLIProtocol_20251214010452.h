#pragma once
#include <string>

// 前向声明
class FSProtocol;
class Authenticator;
class PermissionChecker;
class BackupFlow;
class ReviewFlow;

class CLIProtocol {
public:
    // 构造函数，接收所有它需要与之交互的服务和流程
    CLIProtocol(FSProtocol* fs, Authenticator* auth, PermissionChecker* perm, BackupFlow* backup, ReviewFlow* review);

    // 解析并处理命令
    bool processCommand(const std::string& command, std::string& response);

private:
    FSProtocol* m_fs;
    Authenticator* m_auth;
    PermissionChecker* m_perm;
    BackupFlow* m_backupFlow;
    ReviewFlow* m_reviewFlow;
};