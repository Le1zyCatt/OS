#pragma once
#include <string>
#include <vector>

// 定义用户角色
enum class UserRole {
    ADMIN,
    EDITOR,
    REVIEWER,
    AUTHOR,
    GUEST,
    UNKNOWN
};

// 【身份认证占位】不填充技术细节
class Authenticator {
public:
    // 登录：返回会话令牌 (Session ID)
    virtual std::string login(const std::string& username, 
                                            const std::string& password, 
                                            std::string& errorMsg) = 0;
    
    // 验证会话有效性
    virtual bool validateSession(const std::string& sessionId, 
                               std::string& username, 
                               std::string& errorMsg) = 0;
    
    // 注销会话
    virtual bool logout(const std::string& sessionId, std::string& errorMsg) = 0;

    // 获取用户角色
    virtual UserRole getUserRole(const std::string& sessionToken) = 0;

    // --- 会话级当前目录（用于 CD/PWD）---
    // 说明：server 是“一条命令一个连接”，但 token 代表逻辑会话。
    //      这里把 cwd 挂在会话上，使得多次命令之间可以共享当前目录。
    virtual std::string getCwd(const std::string& sessionToken) = 0;
    virtual bool setCwd(const std::string& sessionToken, const std::string& cwd, std::string& errorMsg) = 0;

    // --- 管理员：用户管理 ---
    virtual bool addUser(const std::string& username,
                         const std::string& password,
                         UserRole role,
                         std::string& errorMsg) = 0;

    virtual bool deleteUser(const std::string& username, std::string& errorMsg) = 0;

    virtual std::vector<std::pair<std::string, UserRole>> listUsers(std::string& errorMsg) = 0;

    // 检查用户是否存在
    virtual bool userExists(const std::string& username) = 0;
    
    // --- 研究方向管理 ---
    // 设置用户的研究方向（字符串列表，如 {"biology", "chemistry"}）
    virtual bool setUserFields(const std::string& username, const std::vector<std::string>& fields, std::string& errorMsg) = 0;
    
    // 获取用户的研究方向
    virtual std::vector<std::string> getUserFields(const std::string& username, std::string& errorMsg) = 0;
    
    // 获取所有具有特定角色的用户及其研究方向
    virtual std::vector<std::pair<std::string, std::vector<std::string>>> listUsersByRole(UserRole role, std::string& errorMsg) = 0;
};