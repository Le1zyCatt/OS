#include "../../include/auth/Authenticator.h"
#include <ctime>
#include <iostream>
#include <memory>

class RealAuthenticator : public Authenticator {
public:
    std::string login(const std::string& username, const std::string& password, std::string& errorMsg) override {
        std::cout << "RealAuthenticator::login called" << std::endl;
        if (username == "admin" && password == "admin123") {
            return "mock_session_id_for_admin";
        }
        return "invalid_session";
    }

    bool validateSession(const std::string& sessionId, std::string& username, std::string& errorMsg) override {
        std::cout << "RealAuthenticator::validateSession called" << std::endl;
        return sessionId.rfind("mock_session_id", 0) == 0; // 简单模拟
    }

    bool logout(const std::string& sessionId, std::string& errorMsg) override {
        std::cout << "RealAuthenticator::logout called" << std::endl;
        return true;
    }

    UserRole getUserRole(const std::string& sessionToken) override {
        std::cout << "RealAuthenticator::getUserRole called" << std::endl;
        if (sessionToken == "mock_session_id_for_admin") {
            return UserRole::ADMIN;
        }
        if (sessionToken == "mock_session_id_for_dev") {
            return UserRole::DEVELOPER;
        }
        return UserRole::GUEST;
    }
};

// 【工厂函数实现】
// 定义 createAuthenticator 函数，这是链接器正在寻找的函数
std::unique_ptr<Authenticator> createAuthenticator() {
    return std::make_unique<RealAuthenticator>();
}