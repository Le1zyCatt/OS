#include "../../include/protocol/CLIProtocol.h"
#include "../../include/protocol/FSProtocol.h"
#include "../../include/auth/Authenticator.h"
#include "../../include/auth/PermissionChecker.h"
#include "../../include/business/BackupFlow.h"
#include "../../include/business/ReviewFlow.h"
#include <iostream>
#include <sstream>
#include <vector>

CLIProtocol::CLIProtocol(FSProtocol* fs, Authenticator* auth, PermissionChecker* perm, BackupFlow* backup, ReviewFlow* review)
    : m_fs(fs), m_auth(auth), m_perm(perm), m_backupFlow(backup), m_reviewFlow(review) {}

bool CLIProtocol::processCommand(const std::string& command, std::string& response) {
    std::stringstream ss(command);
    std::string cmd;
    ss >> cmd;

    std::string errorMsg;

    if (cmd == "LOGIN") {
        std::string user, pass;
        ss >> user >> pass;
        std::string sessionId = m_auth->login(user, pass, errorMsg);
        if (!sessionId.empty()) {
            const UserRole role = m_auth->getUserRole(sessionId);
            std::string roleStr = "UNKNOWN";
            switch (role) {
                case UserRole::ADMIN: roleStr = "ADMIN"; break;
                case UserRole::EDITOR: roleStr = "EDITOR"; break;
                case UserRole::REVIEWER: roleStr = "REVIEWER"; break;
                case UserRole::AUTHOR: roleStr = "AUTHOR"; break;
                case UserRole::GUEST: roleStr = "GUEST"; break;
                default: break;
            }
            response = "OK: " + sessionId + " ROLE=" + roleStr;
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "LOGOUT") {
        std::string sessionId;
        ss >> sessionId;
        if (sessionId.empty()) {
            response = "ERROR: Usage: LOGOUT <sessionToken>";
            return false;
        }
        if (m_auth->logout(sessionId, errorMsg)) {
            response = "OK: Logged out.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "READ") {
        std::string sessionId, path, content;
        ss >> sessionId >> path;
        if (sessionId.empty() || path.empty()) {
            response = "ERROR: Usage: READ <sessionToken> <path>";
            return false;
        }

        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::READ_FILE)) {
            response = "ERROR: Permission denied.";
            return false;
        }

        if (m_fs->readFile(path, content, errorMsg)) {
            response = "OK: " + content;
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "WRITE") {
        std::string sessionId, path, content;
        ss >> sessionId >> path;
        std::getline(ss, content);
        if (!content.empty() && content[0] == ' ') content = content.substr(1);

        if (sessionId.empty() || path.empty()) {
            response = "ERROR: Usage: WRITE <sessionToken> <path> <content>";
            return false;
        }

        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::WRITE_FILE)) {
            response = "ERROR: Permission denied.";
            return false;
        }

        if (m_fs->writeFile(path, content, errorMsg)) {
            response = "OK: File written.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "MKDIR") {
        std::string sessionId, path;
        ss >> sessionId >> path;
        if (sessionId.empty() || path.empty()) {
            response = "ERROR: Usage: MKDIR <sessionToken> <path>";
            return false;
        }

        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::MKDIR)) {
            response = "ERROR: Permission denied.";
            return false;
        }

        if (m_fs->createDirectory(path, errorMsg)) {
            response = "OK: Directory created.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "BACKUP") {
        std::string sessionId, path;
        ss >> sessionId >> path;
        if (sessionId.empty() || path.empty()) {
            response = "ERROR: Usage: BACKUP <sessionToken> <path>";
            return false;
        }
        if (m_backupFlow->createBackup(sessionId, path, errorMsg)) {
            response = "OK: Backup created successfully.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "SUBMIT_REVIEW") {
        std::string sessionId, operation, path;
        ss >> sessionId >> operation >> path;
        if (sessionId.empty() || operation.empty() || path.empty()) {
            response = "ERROR: Usage: SUBMIT_REVIEW <sessionToken> <operation> <path>";
            return false;
        }
        std::string reviewId = m_reviewFlow->submitForReview(sessionId, operation, path, errorMsg);
        if (!reviewId.empty()) {
            response = "OK: Review submitted with ID " + reviewId;
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else {
        response = "ERROR: Unknown command '" + cmd + "'";
        return false;
    }

    return true;
}