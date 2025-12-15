#include "../../include/protocol/CLIProtocol.h"
#include "../../include/protocol/FSProtocol.h"
#include "../../include/auth/Authenticator.h"
#include "../../include/auth/PermissionChecker.h"
#include "../../include/business/BackupFlow.h"
#include "../../include/business/PaperService.h"
#include "../../include/business/ReviewFlow.h"
#include "../../include/cache/CacheStatsProvider.h"
#include <iostream>
#include <sstream>
#include <vector>

namespace {

std::string roleToString(UserRole role) {
    switch (role) {
        case UserRole::ADMIN: return "ADMIN";
        case UserRole::EDITOR: return "EDITOR";
        case UserRole::REVIEWER: return "REVIEWER";
        case UserRole::AUTHOR: return "AUTHOR";
        case UserRole::GUEST: return "GUEST";
        default: return "UNKNOWN";
    }
}

UserRole parseRole(const std::string& s) {
    std::string up = s;
    for (auto& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (up == "ADMIN") return UserRole::ADMIN;
    if (up == "EDITOR") return UserRole::EDITOR;
    if (up == "REVIEWER") return UserRole::REVIEWER;
    if (up == "AUTHOR") return UserRole::AUTHOR;
    if (up == "GUEST") return UserRole::GUEST;
    return UserRole::UNKNOWN;
}

}

CLIProtocol::CLIProtocol(FSProtocol* fs,
                         Authenticator* auth,
                         PermissionChecker* perm,
                         BackupFlow* backup,
                         PaperService* paper,
                                                 ReviewFlow* review,
                                                 ICacheStatsProvider* cacheStatsProvider)
        : m_fs(fs),
            m_auth(auth),
            m_perm(perm),
            m_backupFlow(backup),
            m_paper(paper),
            m_reviewFlow(review),
            m_cacheStatsProvider(cacheStatsProvider) {}

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
            response = "OK: " + sessionId + " ROLE=" + roleToString(role);
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
    } else if (cmd == "HELP") {
        std::string sessionId;
        ss >> sessionId;
        if (sessionId.empty()) {
            response = "OK: Commands: LOGIN, HELP";
            return true;
        }
        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        std::ostringstream oss;
        oss << "OK: ROLE=" << roleToString(role) << "\n";
        oss << "Common: READ WRITE MKDIR STATUS PAPER_DOWNLOAD\n";
        if (role == UserRole::AUTHOR) oss << "Author: PAPER_UPLOAD PAPER_REVISE REVIEWS_DOWNLOAD\n";
        if (role == UserRole::REVIEWER) oss << "Reviewer: REVIEW_SUBMIT\n";
        if (role == UserRole::EDITOR) oss << "Editor: ASSIGN_REVIEWER DECIDE REVIEWS_DOWNLOAD\n";
        if (role == UserRole::ADMIN) oss << "Admin: USER_ADD USER_DEL USER_LIST BACKUP_CREATE BACKUP_LIST BACKUP_RESTORE SYSTEM_STATUS CACHE_STATS CACHE_CLEAR\n";
        response = oss.str();
    } else if (cmd == "CACHE_STATS") {
        std::string sessionId;
        ss >> sessionId;
        if (sessionId.empty()) {
            response = "ERROR: Usage: CACHE_STATS <sessionToken>";
            return false;
        }

        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::SYSTEM_STATUS)) {
            response = "ERROR: Permission denied.";
            return false;
        }
        if (!m_cacheStatsProvider) {
            response = "ERROR: Cache stats not available.";
            return false;
        }

        const CacheStats s = m_cacheStatsProvider->cacheStats();
        std::ostringstream oss;
        oss << "OK: hits=" << s.hits
            << " misses=" << s.misses
            << " size=" << s.size
            << " capacity=" << s.capacity;
        response = oss.str();
    } else if (cmd == "CACHE_CLEAR") {
        std::string sessionId;
        ss >> sessionId;
        if (sessionId.empty()) {
            response = "ERROR: Usage: CACHE_CLEAR <sessionToken>";
            return false;
        }

        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::SYSTEM_STATUS)) {
            response = "ERROR: Permission denied.";
            return false;
        }
        if (!m_cacheStatsProvider) {
            response = "ERROR: Cache stats not available.";
            return false;
        }

        m_cacheStatsProvider->clearCache();
        response = "OK: Cache cleared.";
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
    } else if (cmd == "BACKUP" || cmd == "BACKUP_CREATE") {
        std::string sessionId, path, name;
        ss >> sessionId >> path >> name;
        if (sessionId.empty() || path.empty()) {
            response = "ERROR: Usage: BACKUP_CREATE <sessionToken> <path> [name]";
            return false;
        }
        // name 为空时由 flow 生成默认名称
        if (m_backupFlow->createBackup(sessionId, path, name, errorMsg)) {
            response = "OK: Backup created.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "BACKUP_LIST") {
        std::string sessionId;
        ss >> sessionId;
        if (sessionId.empty()) {
            response = "ERROR: Usage: BACKUP_LIST <sessionToken>";
            return false;
        }
        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::BACKUP_LIST)) {
            response = "ERROR: Permission denied.";
            return false;
        }
        auto names = m_fs->listSnapshots("/", errorMsg);
        if (!errorMsg.empty() && names.empty()) {
            response = "ERROR: " + errorMsg;
            return false;
        }
        std::ostringstream oss;
        oss << "OK:";
        for (const auto& n : names) oss << " " << n;
        response = oss.str();
    } else if (cmd == "BACKUP_RESTORE") {
        std::string sessionId, name;
        ss >> sessionId >> name;
        if (sessionId.empty() || name.empty()) {
            response = "ERROR: Usage: BACKUP_RESTORE <sessionToken> <name>";
            return false;
        }
        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::BACKUP_RESTORE)) {
            response = "ERROR: Permission denied.";
            return false;
        }
        if (m_fs->restoreSnapshot(name, errorMsg)) {
            response = "OK: Restored.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "SYSTEM_STATUS") {
        std::string sessionId;
        ss >> sessionId;
        if (sessionId.empty()) {
            response = "ERROR: Usage: SYSTEM_STATUS <sessionToken>";
            return false;
        }
        std::string username;
        if (!m_auth->validateSession(sessionId, username, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole role = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(role, Permission::SYSTEM_STATUS)) {
            response = "ERROR: Permission denied.";
            return false;
        }
        response = "OK: Server running. (FS stats not available via interface yet)";
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
    } else if (cmd == "PAPER_UPLOAD") {
        std::string sessionId, paperId, content;
        ss >> sessionId >> paperId;
        std::getline(ss, content);
        if (!content.empty() && content[0] == ' ') content = content.substr(1);
        if (sessionId.empty() || paperId.empty()) {
            response = "ERROR: Usage: PAPER_UPLOAD <sessionToken> <paperId> <content>";
            return false;
        }
        if (m_paper->uploadPaper(sessionId, paperId, content, errorMsg)) {
            response = "OK: Paper uploaded.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "PAPER_REVISE") {
        std::string sessionId, paperId, content;
        ss >> sessionId >> paperId;
        std::getline(ss, content);
        if (!content.empty() && content[0] == ' ') content = content.substr(1);
        if (sessionId.empty() || paperId.empty()) {
            response = "ERROR: Usage: PAPER_REVISE <sessionToken> <paperId> <content>";
            return false;
        }
        if (m_paper->submitRevision(sessionId, paperId, content, errorMsg)) {
            response = "OK: Revision submitted.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "PAPER_DOWNLOAD") {
        std::string sessionId, paperId, content;
        ss >> sessionId >> paperId;
        if (sessionId.empty() || paperId.empty()) {
            response = "ERROR: Usage: PAPER_DOWNLOAD <sessionToken> <paperId>";
            return false;
        }
        if (m_paper->downloadPaper(sessionId, paperId, content, errorMsg)) {
            response = "OK: " + content;
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "STATUS") {
        std::string sessionId, paperId, out;
        ss >> sessionId >> paperId;
        if (sessionId.empty() || paperId.empty()) {
            response = "ERROR: Usage: STATUS <sessionToken> <paperId>";
            return false;
        }
        if (m_paper->getStatus(sessionId, paperId, out, errorMsg)) {
            response = "OK:\n" + out;
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "ASSIGN_REVIEWER") {
        std::string sessionId, paperId, reviewer;
        ss >> sessionId >> paperId >> reviewer;
        if (sessionId.empty() || paperId.empty() || reviewer.empty()) {
            response = "ERROR: Usage: ASSIGN_REVIEWER <sessionToken> <paperId> <reviewerUsername>";
            return false;
        }
        if (m_paper->assignReviewer(sessionId, paperId, reviewer, errorMsg)) {
            response = "OK: Reviewer assigned.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "REVIEW_SUBMIT") {
        std::string sessionId, paperId, content;
        ss >> sessionId >> paperId;
        std::getline(ss, content);
        if (!content.empty() && content[0] == ' ') content = content.substr(1);
        if (sessionId.empty() || paperId.empty()) {
            response = "ERROR: Usage: REVIEW_SUBMIT <sessionToken> <paperId> <reviewContent>";
            return false;
        }
        if (m_paper->submitReview(sessionId, paperId, content, errorMsg)) {
            response = "OK: Review submitted.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "REVIEWS_DOWNLOAD") {
        std::string sessionId, paperId, out;
        ss >> sessionId >> paperId;
        if (sessionId.empty() || paperId.empty()) {
            response = "ERROR: Usage: REVIEWS_DOWNLOAD <sessionToken> <paperId>";
            return false;
        }
        if (m_paper->downloadReviews(sessionId, paperId, out, errorMsg)) {
            response = "OK:\n" + out;
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "DECIDE") {
        std::string sessionId, paperId, decision;
        ss >> sessionId >> paperId >> decision;
        if (sessionId.empty() || paperId.empty() || decision.empty()) {
            response = "ERROR: Usage: DECIDE <sessionToken> <paperId> <ACCEPT|REJECT>";
            return false;
        }
        if (m_paper->finalDecision(sessionId, paperId, decision, errorMsg)) {
            response = "OK: Decision recorded.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "USER_ADD") {
        std::string sessionId, username, password, roleStr;
        ss >> sessionId >> username >> password >> roleStr;
        if (sessionId.empty() || username.empty() || password.empty() || roleStr.empty()) {
            response = "ERROR: Usage: USER_ADD <sessionToken> <username> <password> <ADMIN|EDITOR|REVIEWER|AUTHOR|GUEST>";
            return false;
        }
        std::string me;
        if (!m_auth->validateSession(sessionId, me, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole myRole = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(myRole, Permission::USER_MANAGE)) {
            response = "ERROR: Permission denied.";
            return false;
        }
        const UserRole role = parseRole(roleStr);
        if (role == UserRole::UNKNOWN) {
            response = "ERROR: Invalid role.";
            return false;
        }
        if (m_auth->addUser(username, password, role, errorMsg)) {
            response = "OK: User added.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "USER_DEL") {
        std::string sessionId, username;
        ss >> sessionId >> username;
        if (sessionId.empty() || username.empty()) {
            response = "ERROR: Usage: USER_DEL <sessionToken> <username>";
            return false;
        }
        std::string me;
        if (!m_auth->validateSession(sessionId, me, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole myRole = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(myRole, Permission::USER_MANAGE)) {
            response = "ERROR: Permission denied.";
            return false;
        }
        if (m_auth->deleteUser(username, errorMsg)) {
            response = "OK: User deleted.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "USER_LIST") {
        std::string sessionId;
        ss >> sessionId;
        if (sessionId.empty()) {
            response = "ERROR: Usage: USER_LIST <sessionToken>";
            return false;
        }
        std::string me;
        if (!m_auth->validateSession(sessionId, me, errorMsg)) {
            response = "ERROR: Not authenticated: " + errorMsg;
            return false;
        }
        const UserRole myRole = m_auth->getUserRole(sessionId);
        if (!m_perm->hasPermission(myRole, Permission::USER_MANAGE)) {
            response = "ERROR: Permission denied.";
            return false;
        }
        auto users = m_auth->listUsers(errorMsg);
        if (!errorMsg.empty() && users.empty()) {
            response = "ERROR: " + errorMsg;
            return false;
        }
        std::ostringstream oss;
        oss << "OK:";
        for (const auto& [name, role] : users) {
            oss << "\n" << name << " " << roleToString(role);
        }
        response = oss.str();
    } else {
        response = "ERROR: Unknown command '" + cmd + "'";
        return false;
    }

    return true;
}