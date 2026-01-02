#include "../../include/protocol/RealFileSystemAdapter.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

// 引入 filesystem 的 C API
#include "disk.h"
#include "inode.h"
#include "path.h"
#include "block_cache.h"

// ==================== 构造和析构 ====================

RealFileSystemAdapter::RealFileSystemAdapter(const std::string& diskPath) {
    m_fd = disk_open(diskPath.c_str());
    if (m_fd < 0) {
        std::cerr << "❌ Failed to open disk image: " << diskPath << std::endl;
        throw std::runtime_error("Failed to open disk image: " + diskPath);
    }
    
    // 初始化块缓存（容量为 128 个块，约 128KB）
    block_cache_init(128);
    
    std::cout << "✅ Filesystem adapter initialized with disk: " << diskPath << std::endl;
}

RealFileSystemAdapter::~RealFileSystemAdapter() {
    if (m_fd >= 0) {
        // 刷新并销毁块缓存
        block_cache_flush(m_fd);
        block_cache_destroy();
        
        disk_close(m_fd);
        std::cout << "✅ Filesystem adapter closed" << std::endl;
    }
}

// ==================== 辅助函数 ====================

std::string RealFileSystemAdapter::normalizePath(const std::string& path) {
    if (path.empty()) return "/";
    
    std::string normalized = path;
    // 替换反斜杠为正斜杠
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    
    // 确保以 / 开头
    if (normalized.front() != '/') {
        normalized.insert(normalized.begin(), '/');
    }
    
    // 移除末尾的 /（除非是根目录）
    while (normalized.size() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }
    
    return normalized;
}

int RealFileSystemAdapter::pathToInodeId(const std::string& path, std::string& errorMsg) {
    std::string normPath = normalizePath(path);
    
    // 根目录特殊处理
    if (normPath == "/") {
        return 0;  // 根目录的 inode ID 总是 0
    }
    
    int inodeId = get_inode_by_path(m_fd, normPath.c_str());
    if (inodeId < 0) {
        errorMsg = "Path not found: " + normPath;
    }
    return inodeId;
}

bool RealFileSystemAdapter::getParentAndName(const std::string& path, int& parentInodeId, 
                                             std::string& name, std::string& errorMsg) {
    std::string normPath = normalizePath(path);
    
    if (normPath == "/") {
        errorMsg = "Cannot get parent of root directory";
        return false;
    }
    
    char nameBuf[DIR_NAME_SIZE];
    int result = get_parent_inode_and_name(m_fd, normPath.c_str(), &parentInodeId, nameBuf);
    
    if (result < 0) {
        errorMsg = "Failed to get parent directory for: " + normPath;
        return false;
    }
    
    name = std::string(nameBuf);
    return true;
}

bool RealFileSystemAdapter::ensureDirectoryExists(const std::string& path, std::string& errorMsg) {
    std::string normPath = normalizePath(path);
    
    // 根目录总是存在
    if (normPath == "/") {
        return true;
    }
    
    // 检查目录是否已存在
    int inodeId = get_inode_by_path(m_fd, normPath.c_str());
    if (inodeId >= 0) {
        // 目录已存在，检查是否真的是目录
        Inode inode;
        if (read_inode(m_fd, inodeId, &inode) < 0) {
            errorMsg = "Failed to read inode for: " + normPath;
            return false;
        }
        
        if (inode.type != INODE_TYPE_DIR) {
            errorMsg = "Path exists but is not a directory: " + normPath;
            return false;
        }
        
        return true;  // 目录已存在
    }
    
    // 目录不存在，需要创建
    // 首先确保父目录存在
    size_t lastSlash = normPath.find_last_of('/');
    if (lastSlash == std::string::npos) {
        errorMsg = "Invalid path: " + normPath;
        return false;
    }
    
    std::string parentPath = (lastSlash == 0) ? "/" : normPath.substr(0, lastSlash);
    if (!ensureDirectoryExists(parentPath, errorMsg)) {
        return false;
    }
    
    // 创建当前目录
    return createDirectory(normPath, errorMsg);
}

// ==================== FSProtocol 接口实现 ====================

bool RealFileSystemAdapter::createSnapshot(const std::string& path, const std::string& snapshotName, 
                                           std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    (void)path;  // 当前 filesystem 的快照是全局的，不支持路径级快照
    
    if (snapshotName.empty()) {
        errorMsg = "Snapshot name cannot be empty";
        return false;
    }
    
    int result = create_snapshot(m_fd, snapshotName.c_str());
    if (result < 0) {
        errorMsg = "Failed to create snapshot: " + snapshotName;
        return false;
    }
    
    std::cout << "✅ Created snapshot: " << snapshotName << " (ID: " << result << ")" << std::endl;
    return true;
}

bool RealFileSystemAdapter::restoreSnapshot(const std::string& snapshotName, std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (snapshotName.empty()) {
        errorMsg = "Snapshot name cannot be empty";
        return false;
    }
    
    // 查找快照 ID
    Snapshot snapshots[MAX_SNAPSHOTS];
    int count = list_snapshots(m_fd, snapshots, MAX_SNAPSHOTS);
    
    if (count < 0) {
        errorMsg = "Failed to list snapshots";
        return false;
    }
    
    int snapshotId = -1;
    for (int i = 0; i < count; ++i) {
        if (snapshots[i].active && snapshotName == snapshots[i].name) {
            snapshotId = snapshots[i].id;
            break;
        }
    }
    
    if (snapshotId < 0) {
        errorMsg = "Snapshot not found: " + snapshotName;
        return false;
    }
    
    int result = restore_snapshot(m_fd, snapshotId);
    if (result < 0) {
        errorMsg = "Failed to restore snapshot: " + snapshotName;
        return false;
    }
    
    std::cout << "✅ Restored snapshot: " << snapshotName << " (ID: " << snapshotId << ")" << std::endl;
    return true;
}

std::vector<std::string> RealFileSystemAdapter::listSnapshots(const std::string& path, std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    (void)path;  // 当前 filesystem 的快照是全局的
    
    Snapshot snapshots[MAX_SNAPSHOTS];
    int count = list_snapshots(m_fd, snapshots, MAX_SNAPSHOTS);
    
    if (count < 0) {
        errorMsg = "Failed to list snapshots";
        return {};
    }
    
    std::vector<std::string> names;
    for (int i = 0; i < count; ++i) {
        if (snapshots[i].active) {
            names.push_back(snapshots[i].name);
        }
    }
    
    std::sort(names.begin(), names.end());
    return names;
}

bool RealFileSystemAdapter::readFile(const std::string& path, std::string& content, std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string normPath = normalizePath(path);
    
    // 获取文件的 inode ID
    int inodeId = pathToInodeId(normPath, errorMsg);
    if (inodeId < 0) {
        return false;
    }
    
    // 读取 inode
    Inode inode;
    if (read_inode(m_fd, inodeId, &inode) < 0) {
        errorMsg = "Failed to read inode for: " + normPath;
        return false;
    }
    
    // 检查是否是文件
    if (inode.type != INODE_TYPE_FILE) {
        errorMsg = "Path is not a file: " + normPath;
        return false;
    }
    
    // 读取文件内容
    if (inode.size == 0) {
        content = "";
        return true;
    }
    
    std::vector<char> buffer(inode.size);
    int bytesRead = inode_read_data(m_fd, &inode, buffer.data(), 0, inode.size);
    
    if (bytesRead < 0 || bytesRead != inode.size) {
        errorMsg = "Failed to read file data: " + normPath;
        return false;
    }
    
    content.assign(buffer.begin(), buffer.end());
    return true;
}

bool RealFileSystemAdapter::writeFile(const std::string& path, const std::string& content, 
                                      std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string normPath = normalizePath(path);
    
    // 获取父目录和文件名
    int parentInodeId;
    std::string fileName;
    if (!getParentAndName(normPath, parentInodeId, fileName, errorMsg)) {
        return false;
    }
    
    // 确保父目录存在
    size_t lastSlash = normPath.find_last_of('/');
    std::string parentPath = (lastSlash == 0) ? "/" : normPath.substr(0, lastSlash);
    if (!ensureDirectoryExists(parentPath, errorMsg)) {
        return false;
    }
    
    // 重新获取父目录 inode（因为可能刚创建）
    parentInodeId = pathToInodeId(parentPath, errorMsg);
    if (parentInodeId < 0) {
        return false;
    }
    
    // 读取父目录 inode
    Inode parentInode;
    if (read_inode(m_fd, parentInodeId, &parentInode) < 0) {
        errorMsg = "Failed to read parent directory inode";
        return false;
    }
    
    // 检查文件是否已存在
    int fileInodeId = dir_find_entry(m_fd, &parentInode, fileName.c_str());
    
    Inode fileInode;
    if (fileInodeId < 0) {
        // 文件不存在，创建新文件
        fileInodeId = alloc_inode(m_fd);
        if (fileInodeId < 0) {
            errorMsg = "Failed to allocate inode for new file";
            return false;
        }
        
        init_inode(&fileInode, INODE_TYPE_FILE);
        
        // 添加目录条目
        if (dir_add_entry(m_fd, &parentInode, parentInodeId, fileName.c_str(), fileInodeId) < 0) {
            free_inode(m_fd, fileInodeId);
            errorMsg = "Failed to add directory entry";
            return false;
        }
    } else {
        // 文件已存在，读取 inode
        if (read_inode(m_fd, fileInodeId, &fileInode) < 0) {
            errorMsg = "Failed to read existing file inode";
            return false;
        }
        
        if (fileInode.type != INODE_TYPE_FILE) {
            errorMsg = "Path exists but is not a file: " + normPath;
            return false;
        }
        
        // 清空旧内容
        if (fileInode.block_count > 0) {
            inode_free_blocks(m_fd, &fileInode);
        }
    }
    
    // 写入新内容
    if (!content.empty()) {
        int bytesWritten = inode_write_data(m_fd, &fileInode, fileInodeId, 
                                            content.c_str(), 0, content.length());
        if (bytesWritten < 0 || bytesWritten != static_cast<int>(content.length())) {
            errorMsg = "Failed to write file data";
            return false;
        }
    } else {
        // 空文件，只需要更新 inode
        fileInode.size = 0;
        write_inode(m_fd, fileInodeId, &fileInode);
    }
    
    return true;
}

bool RealFileSystemAdapter::deleteFile(const std::string& path, std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string normPath = normalizePath(path);
    
    // 不能删除根目录
    if (normPath == "/") {
        errorMsg = "Cannot delete root directory";
        return false;
    }
    
    // 获取父目录和文件名
    int parentInodeId;
    std::string fileName;
    if (!getParentAndName(normPath, parentInodeId, fileName, errorMsg)) {
        return false;
    }
    
    // 读取父目录 inode
    Inode parentInode;
    if (read_inode(m_fd, parentInodeId, &parentInode) < 0) {
        errorMsg = "Failed to read parent directory inode";
        return false;
    }
    
    // 查找文件
    int fileInodeId = dir_find_entry(m_fd, &parentInode, fileName.c_str());
    if (fileInodeId < 0) {
        errorMsg = "File not found: " + normPath;
        return false;
    }
    
    // 读取文件 inode
    Inode fileInode;
    if (read_inode(m_fd, fileInodeId, &fileInode) < 0) {
        errorMsg = "Failed to read file inode";
        return false;
    }
    
    // 只能删除文件，不能删除目录（需要单独的 rmdir）
    if (fileInode.type == INODE_TYPE_DIR) {
        errorMsg = "Cannot delete directory using deleteFile: " + normPath;
        return false;
    }
    
    // 释放数据块
    if (fileInode.block_count > 0) {
        inode_free_blocks(m_fd, &fileInode);
    }
    
    // 释放 inode
    free_inode(m_fd, fileInodeId);
    
    // 从父目录中删除条目
    if (dir_remove_entry(m_fd, &parentInode, parentInodeId, fileName.c_str()) < 0) {
        errorMsg = "Failed to remove directory entry";
        return false;
    }
    
    return true;
}

bool RealFileSystemAdapter::createDirectory(const std::string& path, std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string normPath = normalizePath(path);
    
    // 根目录已存在
    if (normPath == "/") {
        return true;
    }
    
    // 检查目录是否已存在
    int existingInodeId = get_inode_by_path(m_fd, normPath.c_str());
    if (existingInodeId >= 0) {
        errorMsg = "Directory already exists: " + normPath;
        return false;
    }
    
    // 获取父目录和目录名
    int parentInodeId;
    std::string dirName;
    if (!getParentAndName(normPath, parentInodeId, dirName, errorMsg)) {
        return false;
    }
    
    // 读取父目录 inode
    Inode parentInode;
    if (read_inode(m_fd, parentInodeId, &parentInode) < 0) {
        errorMsg = "Failed to read parent directory inode";
        return false;
    }
    
    // 分配新的 inode
    int newDirInodeId = alloc_inode(m_fd);
    if (newDirInodeId < 0) {
        errorMsg = "Failed to allocate inode for new directory";
        return false;
    }
    
    // 初始化目录 inode
    Inode newDirInode;
    init_inode(&newDirInode, INODE_TYPE_DIR);
    write_inode(m_fd, newDirInodeId, &newDirInode);
    
    // 添加到父目录
    if (dir_add_entry(m_fd, &parentInode, parentInodeId, dirName.c_str(), newDirInodeId) < 0) {
        free_inode(m_fd, newDirInodeId);
        errorMsg = "Failed to add directory entry";
        return false;
    }
    
    return true;
}

std::string RealFileSystemAdapter::getFilePermission(const std::string& path, const std::string& user, 
                                                     std::string& errorMsg) {
    // 权限管理由 Server 层的 PermissionChecker 处理
    // FileSystem 层不维护权限信息
    (void)path;
    (void)user;
    (void)errorMsg;
    return "rwx";  // 默认返回全部权限
}

std::string RealFileSystemAdapter::submitForReview(const std::string& operation, const std::string& path, 
                                                   const std::string& user, std::string& errorMsg) {
    // 审核流程由 Server 层处理
    // FileSystem 层不维护审核信息
    (void)operation;
    (void)path;
    (void)user;
    (void)errorMsg;
    return "OK";  // 占位返回
}

