#include "../../include/auth/Authenticator.h"
#include <iostream>
#include <memory>

#include <chrono>
#include <algorithm>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {

using Clock = std::chrono::steady_clock;

struct UserRecord {
    std::string password;
    UserRole role;
    std::vector<std::string> fields;  // 研究方向列表，如 {"computer_science", "machine_learning"}
};

struct SessionRecord {
    std::string username;
    UserRole role;
    Clock::time_point expiresAt;
    std::string cwd;
};

constexpr std::chrono::minutes kSessionTtl{120};

std::string generateToken() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> dist;

    std::ostringstream oss;
    oss << std::hex;
    oss << dist(rng);
    oss << dist(rng);
    return oss.str();
}

}

class RealAuthenticator : public Authenticator {
public:
    std::string login(const std::string& username, const std::string& password, std::string& errorMsg) override {
        std::scoped_lock lock(m_mutex);

        auto it = m_users.find(username);
        if (it == m_users.end()) {
            errorMsg = "Unknown user.";
            return {};
        }

        if (it->second.password != password) {
            errorMsg = "Invalid password.";
            return {};
        }

        const std::string token = generateToken();
        m_sessions[token] = SessionRecord{
            username,
            it->second.role,
            Clock::now() + kSessionTtl,
            "/",
        };

        return token;
    }

    bool validateSession(const std::string& sessionId, std::string& username, std::string& errorMsg) override {
        std::scoped_lock lock(m_mutex);

        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end()) {
            errorMsg = "Session not found.";
            return false;
        }

        if (Clock::now() > it->second.expiresAt) {
            m_sessions.erase(it);
            errorMsg = "Session expired.";
            return false;
        }

        // sliding expiration：每次验证都续期
        it->second.expiresAt = Clock::now() + kSessionTtl;
        username = it->second.username;
        return true;
    }

    bool logout(const std::string& sessionId, std::string& errorMsg) override {
        std::scoped_lock lock(m_mutex);

        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end()) {
            errorMsg = "Session not found.";
            return false;
        }

        m_sessions.erase(it);
        return true;
    }

    UserRole getUserRole(const std::string& sessionToken) override {
        std::scoped_lock lock(m_mutex);

        auto it = m_sessions.find(sessionToken);
        if (it == m_sessions.end()) {
            return UserRole::UNKNOWN;
        }

        if (Clock::now() > it->second.expiresAt) {
            m_sessions.erase(it);
            return UserRole::UNKNOWN;
        }

        return it->second.role;
    }

    std::string getCwd(const std::string& sessionToken) override {
        std::scoped_lock lock(m_mutex);
        auto it = m_sessions.find(sessionToken);
        if (it == m_sessions.end()) {
            return "/";
        }
        if (Clock::now() > it->second.expiresAt) {
            m_sessions.erase(it);
            return "/";
        }
        return it->second.cwd.empty() ? "/" : it->second.cwd;
    }

    bool setCwd(const std::string& sessionToken, const std::string& cwd, std::string& errorMsg) override {
        std::scoped_lock lock(m_mutex);

        auto it = m_sessions.find(sessionToken);
        if (it == m_sessions.end()) {
            errorMsg = "Session not found.";
            return false;
        }

        if (Clock::now() > it->second.expiresAt) {
            m_sessions.erase(it);
            errorMsg = "Session expired.";
            return false;
        }

        it->second.cwd = cwd.empty() ? "/" : cwd;
        return true;
    }

    bool addUser(const std::string& username,
                 const std::string& password,
                 UserRole role,
                 std::string& errorMsg) override {
        if (username.empty() || password.empty()) {
            errorMsg = "username/password is empty.";
            return false;
        }

        std::scoped_lock lock(m_mutex);
        if (m_users.find(username) != m_users.end()) {
            errorMsg = "User already exists.";
            return false;
        }
        m_users[username] = UserRecord{password, role};
        return true;
    }

    bool deleteUser(const std::string& username, std::string& errorMsg) override {
        if (username.empty()) {
            errorMsg = "username is empty.";
            return false;
        }

        std::scoped_lock lock(m_mutex);
        auto it = m_users.find(username);
        if (it == m_users.end()) {
            errorMsg = "User not found.";
            return false;
        }

        m_users.erase(it);

        // 清理该用户的会话
        for (auto sit = m_sessions.begin(); sit != m_sessions.end();) {
            if (sit->second.username == username) sit = m_sessions.erase(sit);
            else ++sit;
        }

        return true;
    }

    std::vector<std::pair<std::string, UserRole>> listUsers(std::string& errorMsg) override {
        (void)errorMsg;
        std::scoped_lock lock(m_mutex);
        std::vector<std::pair<std::string, UserRole>> out;
        out.reserve(m_users.size());
        for (const auto& [name, rec] : m_users) {
            out.emplace_back(name, rec.role);
        }
        std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        return out;
    }

    bool userExists(const std::string& username) override {
        std::scoped_lock lock(m_mutex);
        return m_users.find(username) != m_users.end();
    }
    
    bool setUserFields(const std::string& username, const std::vector<std::string>& fields, std::string& errorMsg) override {
        std::scoped_lock lock(m_mutex);
        auto it = m_users.find(username);
        if (it == m_users.end()) {
            errorMsg = "User not found.";
            return false;
        }
        it->second.fields = fields;
        return true;
    }
    
    std::vector<std::string> getUserFields(const std::string& username, std::string& errorMsg) override {
        std::scoped_lock lock(m_mutex);
        auto it = m_users.find(username);
        if (it == m_users.end()) {
            errorMsg = "User not found.";
            return {};
        }
        return it->second.fields;
    }
    
    std::vector<std::pair<std::string, std::vector<std::string>>> listUsersByRole(UserRole role, std::string& errorMsg) override {
        (void)errorMsg;
        std::scoped_lock lock(m_mutex);
        std::vector<std::pair<std::string, std::vector<std::string>>> result;
        for (const auto& [name, rec] : m_users) {
            if (rec.role == role) {
                result.emplace_back(name, rec.fields);
            }
        }
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        return result;
    }

private:
    std::mutex m_mutex;
    std::unordered_map<std::string, UserRecord> m_users{
        // 先用硬编码账号便于联调；后续可替换为从 FSProtocol 读取用户/角色文件
        // fields 示例: cs=计算机, bio=生物, chem=化学, physics=物理, math=数学, med=医学, ai=人工智能
        {"admin", {"admin123", UserRole::ADMIN, {}}},
        {"editor", {"editor123", UserRole::EDITOR, {"cs", "ai", "bio"}}},
        {"reviewer", {"reviewer123", UserRole::REVIEWER, {"cs", "ai"}}},
        {"reviewer2", {"reviewer123", UserRole::REVIEWER, {"bio", "med"}}},
        {"reviewer3", {"reviewer123", UserRole::REVIEWER, {"physics", "math"}}},
        {"author", {"author123", UserRole::AUTHOR, {"cs", "ai"}}},
        {"author2", {"author123", UserRole::AUTHOR, {"bio", "chem"}}},
        {"guest", {"guest", UserRole::GUEST, {}}},
    };
    std::unordered_map<std::string, SessionRecord> m_sessions;
};

// 【工厂函数实现】
// 定义 createAuthenticator 函数，这是链接器正在寻找的函数
std::unique_ptr<Authenticator> createAuthenticator() {
    return std::make_unique<RealAuthenticator>();
}