#include "../../include/protocol/CLIProtocol.h"
#include "../../include/protocol/FSProtocol.h"
#include "../../include/auth/Authenticator.h"
#include "../../include/business/BackupFlow.h"
#include "../../include/business/ReviewFlow.h"
#include <iostream>
#include <sstream>
#include <vector>

CLIProtocol::CLIProtocol(FSProtocol* fs, Authenticator* auth, BackupFlow* backup, ReviewFlow* review)
    : m_fs(fs), m_auth(auth), m_backupFlow(backup), m_reviewFlow(review) {}

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
            response = "OK: " + sessionId;
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "READ") {
        std::string path, content;
        ss >> path;
        if (m_fs->readFile(path, content, errorMsg)) {
            response = "OK: " + content;
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "WRITE") {
        std::string path, content;
        ss >> path;
        std::getline(ss, content);
        if (!content.empty() && content[0] == ' ') content = content.substr(1);
        if (m_fs->writeFile(path, content, errorMsg)) {
            response = "OK: File written.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "MKDIR") {
        std::string path;
        ss >> path;
        if (m_fs->createDirectory(path, errorMsg)) {
            response = "OK: Directory created.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "BACKUP") {
        std::string sessionId, path;
        ss >> sessionId >> path;
        if (m_backupFlow->createBackup(sessionId, path, errorMsg)) {
            response = "OK: Backup created successfully.";
        } else {
            response = "ERROR: " + errorMsg;
        }
    } else if (cmd == "SUBMIT_REVIEW") {
        std::string sessionId, operation, path;
        ss >> sessionId >> operation >> path;
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