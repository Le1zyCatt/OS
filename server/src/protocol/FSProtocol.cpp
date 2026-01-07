#include "../../include/protocol/FSProtocol.h"
#include <memory>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../../include/cache/LRUCache.h"
#include "../../include/cache/CacheStatsProvider.h"

namespace {

std::string normalizePath(std::string path) {
    if (path.empty()) return "/";
    std::replace(path.begin(), path.end(), '\\', '/');
    if (path.front() != '/') path.insert(path.begin(), '/');
    // 移除末尾多余的 '/'
    while (path.size() > 1 && path.back() == '/') path.pop_back();
    return path;
}

std::string parentDir(const std::string& path) {
    const auto pos = path.find_last_of('/');
    if (pos == std::string::npos || pos == 0) return "/";
    return path.substr(0, pos);
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::string makeId(const char* prefix) {
    using Clock = std::chrono::system_clock;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
    std::ostringstream oss;
    oss << prefix << ms;
    return oss.str();
}

}

// 快照元数据结构
struct SnapshotMetadata {
    std::unordered_map<std::string, std::string> files;   // 所有文件内容
    std::unordered_set<std::string> dirs;                  // 所有目录
    std::string currentDir;                                 // 当前工作目录
    int timestamp;                                          // 创建时间戳
    size_t totalSize;                                       // 总大小（字节）
    size_t fileCount;                                       // 文件数量
    // 硬链接和打开计数也需要保存
    std::unordered_map<std::string, int> linkCounts;       // 硬链接计数
    std::unordered_map<std::string, int> openCounts;       // 打开计数
    std::unordered_map<std::string, std::string> hardLinks; // 硬链接映射（链接路径 -> 目标路径）
};

// 文件元数据结构
struct FileMeta {
    int linkCount;    // 硬链接计数
    int openCount;    // 打开计数
    FileMeta() : linkCount(1), openCount(0) {}
};

// 具体的文件系统协议实现类
class RealFSProtocol : public FSProtocol {
public:
    RealFSProtocol() {
        // 初始化根目录
        m_dirs.insert("/");
        m_currentDir = "/";
    }

    bool createSnapshot(const std::string& path, const std::string& snapshotName, std::string& errorMsg) override {
        (void)path;  // 全量快照，忽略路径参数
        
        if (snapshotName.empty()) {
            errorMsg = "snapshotName is empty.";
            return false;
        }

        std::scoped_lock lock(m_mutex);
        
        // 创建全量快照：保存所有文件、目录和当前目录
        SnapshotMetadata snapshot;
        snapshot.files = m_files;           // 复制所有文件
        snapshot.dirs = m_dirs;             // 复制所有目录
        snapshot.currentDir = m_currentDir; // 保存当前目录
        snapshot.timestamp = static_cast<int>(std::time(nullptr));
        snapshot.fileCount = m_files.size();
        
        // 保存硬链接和打开计数信息
        for (const auto& [path, meta] : m_fileMeta) {
            snapshot.linkCounts[path] = meta.linkCount;
            snapshot.openCounts[path] = meta.openCount;
        }
        snapshot.hardLinks = m_hardLinks;
        
        // 计算总大小
        size_t totalSize = 0;
        for (const auto& [_, content] : m_files) {
            totalSize += content.size();
        }
        snapshot.totalSize = totalSize;
        
        m_snapshots[snapshotName] = std::move(snapshot);
        
        // 输出快照信息用于调试
        std::cout << "Created snapshot '" << snapshotName << "': " 
                  << m_files.size() << " files, " 
                  << m_dirs.size() << " dirs, "
                  << totalSize << " bytes" << std::endl;
        
        return true;
    }

    bool restoreSnapshot(const std::string& snapshotName, std::string& errorMsg) override {
        std::scoped_lock lock(m_mutex);
        auto it = m_snapshots.find(snapshotName);
        if (it == m_snapshots.end()) {
            errorMsg = "Snapshot not found.";
            return false;
        }

        const SnapshotMetadata& snapshot = it->second;
        
        // 全量恢复：完全替换当前状态
        m_files = snapshot.files;           // 完全替换所有文件
        m_dirs = snapshot.dirs;             // 完全替换所有目录
        m_currentDir = snapshot.currentDir; // 恢复当前目录
        
        // 恢复硬链接和打开计数信息
        m_fileMeta.clear();
        for (const auto& [path, linkCount] : snapshot.linkCounts) {
            m_fileMeta[path].linkCount = linkCount;
        }
        for (const auto& [path, openCount] : snapshot.openCounts) {
            m_fileMeta[path].openCount = openCount;
        }
        m_hardLinks = snapshot.hardLinks;
        
        std::cout << "Restored snapshot '" << snapshotName << "': " 
                  << m_files.size() << " files, " 
                  << m_dirs.size() << " dirs restored" << std::endl;

        return true;
    }

    std::vector<std::string> listSnapshots(const std::string& path, std::string& errorMsg) override {
        (void)path;
        (void)errorMsg;
        std::scoped_lock lock(m_mutex);
        std::vector<std::string> names;
        names.reserve(m_snapshots.size());
        for (const auto& [name, _] : m_snapshots) {
            names.push_back(name);
        }
        std::sort(names.begin(), names.end());
        return names;
    }
    
    // 获取快照详细信息
    bool getSnapshotInfo(const std::string& snapshotName, int& fileCount, size_t& totalSize, std::string& timestamp, std::string& errorMsg) override {
        std::scoped_lock lock(m_mutex);
        auto it = m_snapshots.find(snapshotName);
        if (it == m_snapshots.end()) {
            errorMsg = "Snapshot not found: " + snapshotName;
            return false;
        }
        fileCount = static_cast<int>(it->second.fileCount);
        totalSize = it->second.totalSize;
        // 转换时间戳为字符串
        time_t t = static_cast<time_t>(it->second.timestamp);
        char buf[64];
#ifdef _WIN32
        struct tm tm_buf;
        localtime_s(&tm_buf, &t);
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
#else
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
#endif
        timestamp = buf;
        return true;
    }

    bool readFile(const std::string& path, std::string& content, std::string& errorMsg) override {
        const std::string normPath = normalizePath(path);
        std::scoped_lock lock(m_mutex);

        // 检查是否是硬链接，如果是则读取目标文件
        std::string targetPath = normPath;
        auto linkIt = m_hardLinks.find(normPath);
        if (linkIt != m_hardLinks.end()) {
            targetPath = linkIt->second;
        }

        auto it = m_files.find(targetPath);
        if (it == m_files.end()) {
            errorMsg = "File not found.";
            return false;
        }
        content = it->second;
        return true;
    }

    bool writeFile(const std::string& path, const std::string& content, std::string& errorMsg) override {
        const std::string normPath = normalizePath(path);
        const std::string dir = parentDir(normPath);
        std::scoped_lock lock(m_mutex);

        // 检查是否是硬链接，如果是则写入目标文件
        std::string targetPath = normPath;
        auto linkIt = m_hardLinks.find(normPath);
        if (linkIt != m_hardLinks.end()) {
            targetPath = linkIt->second;
        }

        // 检查文件是否被打开
        auto metaIt = m_fileMeta.find(targetPath);
        if (metaIt != m_fileMeta.end() && metaIt->second.openCount > 0) {
            errorMsg = "File is open by " + std::to_string(metaIt->second.openCount) + " process(es). Cannot modify.";
            return false;
        }

        // 演示用：自动创建父目录
        m_dirs.insert(dir);
        
        // 如果是新文件，初始化元数据
        if (m_files.find(targetPath) == m_files.end()) {
            m_fileMeta[targetPath] = FileMeta();
        }
        
        m_files[targetPath] = content;
        (void)errorMsg;
        return true;
    }

    bool deleteFile(const std::string& path, std::string& errorMsg) override {
        const std::string normPath = normalizePath(path);
        std::scoped_lock lock(m_mutex);

        // 检查是否是硬链接
        auto linkIt = m_hardLinks.find(normPath);
        if (linkIt != m_hardLinks.end()) {
            // 删除硬链接
            std::string targetPath = linkIt->second;
            m_hardLinks.erase(linkIt);
            
            // 减少目标文件的链接计数
            auto metaIt = m_fileMeta.find(targetPath);
            if (metaIt != m_fileMeta.end()) {
                metaIt->second.linkCount--;
            }
            return true;
        }

        auto it = m_files.find(normPath);
        if (it == m_files.end()) {
            errorMsg = "File not found.";
            return false;
        }

        // 检查文件是否被打开
        auto metaIt = m_fileMeta.find(normPath);
        if (metaIt != m_fileMeta.end()) {
            if (metaIt->second.openCount > 0) {
                errorMsg = "File is open by " + std::to_string(metaIt->second.openCount) + " process(es). Cannot delete.";
                return false;
            }
            
            // 检查链接计数
            if (metaIt->second.linkCount > 1) {
                // 还有其他硬链接指向此文件，只减少链接计数
                metaIt->second.linkCount--;
                // 不删除实际文件内容
                return true;
            }
        }

        m_files.erase(normPath);
        m_fileMeta.erase(normPath);
        return true;
    }

    bool createDirectory(const std::string& path, std::string& errorMsg) override {
        const std::string normPath = normalizePath(path);
        std::scoped_lock lock(m_mutex);
        m_dirs.insert(normPath);
        (void)errorMsg;
        return true;
    }

    std::string getFilePermission(const std::string& path, const std::string& user, std::string& errorMsg) override {
        (void)path;
        (void)user;
        (void)errorMsg;
        // 目前权限由 server 的 PermissionChecker 统一管理；FS层接口先返回占位。
        return "managed_by_server";
    }

    std::string submitForReview(const std::string& operation, const std::string& path, 
                                       const std::string& user, std::string& errorMsg) override {
        if (operation.empty()) {
            errorMsg = "operation is empty.";
            return {};
        }

        const std::string normPath = normalizePath(path);
        std::scoped_lock lock(m_mutex);

        const std::string reviewId = makeId("review_");
        m_reviews[reviewId] = ReviewRequest{operation, normPath, user};
        (void)errorMsg;
        return reviewId;
    }

    bool listDirectory(const std::string& path, std::vector<std::string>& entries, std::string& errorMsg) override {
        const std::string normPath = normalizePath(path);
        std::scoped_lock lock(m_mutex);

        if (m_dirs.find(normPath) == m_dirs.end()) {
            errorMsg = "Directory not found: " + normPath;
            return false;
        }

        std::unordered_set<std::string> seen;
        std::vector<std::string> out;

        // 子目录
        for (const auto& d : m_dirs) {
            if (d == "/") continue;
            if (parentDir(d) != normPath) continue;
            const auto pos = d.find_last_of('/');
            const std::string name = (pos == std::string::npos) ? d : d.substr(pos + 1);
            if (name.empty()) continue;
            const std::string decorated = name + "/";
            if (seen.insert(decorated).second) out.push_back(decorated);
        }

        // 子文件
        for (const auto& [p, _content] : m_files) {
            if (parentDir(p) != normPath) continue;
            const auto pos = p.find_last_of('/');
            const std::string name = (pos == std::string::npos) ? p : p.substr(pos + 1);
            if (name.empty()) continue;
            if (seen.insert(name).second) out.push_back(name);
        }

        std::sort(out.begin(), out.end());
        entries = std::move(out);
        return true;
    }

    bool isDirectory(const std::string& path, bool& isDirOut, std::string& errorMsg) override {
        const std::string normPath = normalizePath(path);
        std::scoped_lock lock(m_mutex);

        if (m_dirs.find(normPath) != m_dirs.end()) {
            isDirOut = true;
            return true;
        }
        if (m_files.find(normPath) != m_files.end()) {
            isDirOut = false;
            return true;
        }
        errorMsg = "Path not found: " + normPath;
        return false;
    }

    bool getFileSystemStats(FileSystemStats& stats, std::string& errorMsg) override {
        (void)errorMsg;
        std::scoped_lock lock(m_mutex);
        
        // 模拟的文件系统统计信息
        stats.block_size = 1024;
        stats.block_count = 8192;
        stats.inode_count = 1024;
        stats.free_inode_count = static_cast<int>(1024 - m_files.size() - m_dirs.size());
        stats.free_block_count = static_cast<int>(8192 - 123 - m_files.size() * 2);  // 粗略估计
        stats.data_block_start = 123;
        stats.snapshot_count = static_cast<int>(m_snapshots.size());
        stats.is_real_fs = false;  // 这是模拟的文件系统
        
        return true;
    }

    bool assignReviewer(const std::string& reviewId, const std::string& reviewer, std::string& errorMsg) override {
        std::scoped_lock lock(m_mutex);
        
        auto it = m_reviews.find(reviewId);
        if (it == m_reviews.end()) {
            errorMsg = "Review not found: " + reviewId;
            return false;
        }
        
        // 记录审稿人分配（这里简单地存储，实际应用中可能需要更复杂的逻辑）
        m_reviewAssignments[reviewId] = reviewer;
        return true;
    }

    bool createHardLink(const std::string& sourcePath, const std::string& linkPath, std::string& errorMsg) override {
        const std::string normSource = normalizePath(sourcePath);
        const std::string normLink = normalizePath(linkPath);
        std::scoped_lock lock(m_mutex);

        // 检查源文件是否存在
        if (m_files.find(normSource) == m_files.end()) {
            errorMsg = "Source file not found: " + normSource;
            return false;
        }

        // 检查目标路径是否已存在
        if (m_files.find(normLink) != m_files.end() || m_hardLinks.find(normLink) != m_hardLinks.end()) {
            errorMsg = "Link path already exists: " + normLink;
            return false;
        }

        // 不允许对目录创建硬链接
        if (m_dirs.find(normSource) != m_dirs.end()) {
            errorMsg = "Cannot create hard link to directory.";
            return false;
        }

        // 创建硬链接
        m_hardLinks[normLink] = normSource;
        
        // 增加源文件的链接计数
        if (m_fileMeta.find(normSource) == m_fileMeta.end()) {
            m_fileMeta[normSource] = FileMeta();
        }
        m_fileMeta[normSource].linkCount++;

        return true;
    }

    bool getFileInfo(const std::string& path, FileInfo& info, std::string& errorMsg) override {
        const std::string normPath = normalizePath(path);
        std::scoped_lock lock(m_mutex);

        // 检查是否是目录
        if (m_dirs.find(normPath) != m_dirs.end()) {
            info.path = normPath;
            info.link_count = 1;
            info.open_count = 0;
            info.size = 0;
            info.is_directory = true;
            return true;
        }

        // 检查是否是硬链接
        std::string targetPath = normPath;
        auto linkIt = m_hardLinks.find(normPath);
        if (linkIt != m_hardLinks.end()) {
            targetPath = linkIt->second;
        }

        // 检查是否是文件
        auto it = m_files.find(targetPath);
        if (it == m_files.end()) {
            errorMsg = "File not found: " + normPath;
            return false;
        }

        info.path = normPath;
        info.size = it->second.size();
        info.is_directory = false;

        // 获取元数据
        auto metaIt = m_fileMeta.find(targetPath);
        if (metaIt != m_fileMeta.end()) {
            info.link_count = metaIt->second.linkCount;
            info.open_count = metaIt->second.openCount;
        } else {
            info.link_count = 1;
            info.open_count = 0;
        }

        return true;
    }

    bool openFile(const std::string& path, std::string& errorMsg) override {
        const std::string normPath = normalizePath(path);
        std::scoped_lock lock(m_mutex);

        // 检查是否是硬链接
        std::string targetPath = normPath;
        auto linkIt = m_hardLinks.find(normPath);
        if (linkIt != m_hardLinks.end()) {
            targetPath = linkIt->second;
        }

        // 检查文件是否存在
        if (m_files.find(targetPath) == m_files.end()) {
            errorMsg = "File not found: " + normPath;
            return false;
        }

        // 增加打开计数
        if (m_fileMeta.find(targetPath) == m_fileMeta.end()) {
            m_fileMeta[targetPath] = FileMeta();
        }
        m_fileMeta[targetPath].openCount++;

        return true;
    }

    bool closeFile(const std::string& path, std::string& errorMsg) override {
        const std::string normPath = normalizePath(path);
        std::scoped_lock lock(m_mutex);

        // 检查是否是硬链接
        std::string targetPath = normPath;
        auto linkIt = m_hardLinks.find(normPath);
        if (linkIt != m_hardLinks.end()) {
            targetPath = linkIt->second;
        }

        // 检查文件是否存在
        if (m_files.find(targetPath) == m_files.end()) {
            errorMsg = "File not found: " + normPath;
            return false;
        }

        // 检查打开计数
        auto metaIt = m_fileMeta.find(targetPath);
        if (metaIt == m_fileMeta.end() || metaIt->second.openCount <= 0) {
            errorMsg = "File is not open: " + normPath;
            return false;
        }

        // 减少打开计数
        metaIt->second.openCount--;

        return true;
    }

private:
    struct ReviewRequest {
        std::string operation;
        std::string path;
        std::string user;
    };

    std::mutex m_mutex;
    std::unordered_set<std::string> m_dirs;
    std::unordered_map<std::string, std::string> m_files;
    std::string m_currentDir;
    // snapshotName -> SnapshotMetadata (全量快照)
    std::unordered_map<std::string, SnapshotMetadata> m_snapshots;
    std::unordered_map<std::string, ReviewRequest> m_reviews;
    std::unordered_map<std::string, std::string> m_reviewAssignments;  // reviewId -> reviewer
    // 文件元数据（链接计数、打开计数）
    std::unordered_map<std::string, FileMeta> m_fileMeta;
    // 硬链接映射：链接路径 -> 目标文件路径
    std::unordered_map<std::string, std::string> m_hardLinks;
};

class CachingFSProtocol : public FSProtocol, public ICacheStatsProvider {
public:
    explicit CachingFSProtocol(std::unique_ptr<FSProtocol> inner, size_t capacity)
        : m_inner(std::move(inner)), m_cache(capacity), m_capacity(capacity) {}

    CacheStats cacheStats() const override {
        // LRUCache 内部已加锁，无需外部锁
        return CacheStats{
            m_cache.hits(),
            m_cache.misses(),
            m_cache.size(),
            m_capacity,
        };
    }

    void clearCache() override {
        // LRUCache 内部已加锁，无需外部锁
        m_cache.clear();
    }

    bool createSnapshot(const std::string& path, const std::string& snapshotName, std::string& errorMsg) override {
        return m_inner->createSnapshot(path, snapshotName, errorMsg);
    }

    bool restoreSnapshot(const std::string& snapshotName, std::string& errorMsg) override {
        // 恢复会大规模改变内容：直接清空缓存
        // LRUCache 内部已加锁，无需外部锁
        m_cache.clear();
        return m_inner->restoreSnapshot(snapshotName, errorMsg);
    }

    std::vector<std::string> listSnapshots(const std::string& path, std::string& errorMsg) override {
        return m_inner->listSnapshots(path, errorMsg);
    }

    bool readFile(const std::string& path, std::string& content, std::string& errorMsg) override {
        const std::string key = normalizePath(path);

        // LRUCache 内部已加锁，无需外部锁
        if (auto v = m_cache.tryGet(key)) {
            content = *v;
            return true;
        }

        if (!m_inner->readFile(key, content, errorMsg)) {
            return false;
        }

        // LRUCache 内部已加锁，无需外部锁
        m_cache.put(key, content);
        return true;
    }

    bool writeFile(const std::string& path, const std::string& content, std::string& errorMsg) override {
        const std::string key = normalizePath(path);
        if (!m_inner->writeFile(key, content, errorMsg)) return false;
        // LRUCache 内部已加锁，无需外部锁
        m_cache.put(key, content);
        return true;
    }

    bool deleteFile(const std::string& path, std::string& errorMsg) override {
        const std::string key = normalizePath(path);
        // LRUCache 内部已加锁，无需外部锁
        m_cache.erase(key);
        return m_inner->deleteFile(key, errorMsg);
    }

    bool createDirectory(const std::string& path, std::string& errorMsg) override {
        return m_inner->createDirectory(path, errorMsg);
    }

    std::string getFilePermission(const std::string& path, const std::string& user, std::string& errorMsg) override {
        return m_inner->getFilePermission(path, user, errorMsg);
    }

    std::string submitForReview(const std::string& operation, const std::string& path,
                                const std::string& user, std::string& errorMsg) override {
        return m_inner->submitForReview(operation, path, user, errorMsg);
    }

    bool listDirectory(const std::string& path, std::vector<std::string>& entries, std::string& errorMsg) override {
        return m_inner->listDirectory(path, entries, errorMsg);
    }

    bool isDirectory(const std::string& path, bool& isDirOut, std::string& errorMsg) override {
        return m_inner->isDirectory(path, isDirOut, errorMsg);
    }

    bool getFileSystemStats(FileSystemStats& stats, std::string& errorMsg) override {
        return m_inner->getFileSystemStats(stats, errorMsg);
    }

    bool assignReviewer(const std::string& reviewId, const std::string& reviewer, std::string& errorMsg) override {
        return m_inner->assignReviewer(reviewId, reviewer, errorMsg);
    }

    bool createHardLink(const std::string& sourcePath, const std::string& linkPath, std::string& errorMsg) override {
        return m_inner->createHardLink(sourcePath, linkPath, errorMsg);
    }

    bool getFileInfo(const std::string& path, FileInfo& info, std::string& errorMsg) override {
        return m_inner->getFileInfo(path, info, errorMsg);
    }

    bool openFile(const std::string& path, std::string& errorMsg) override {
        return m_inner->openFile(path, errorMsg);
    }

    bool closeFile(const std::string& path, std::string& errorMsg) override {
        return m_inner->closeFile(path, errorMsg);
    }

    bool getSnapshotInfo(const std::string& snapshotName, int& fileCount, size_t& totalSize, std::string& timestamp, std::string& errorMsg) override {
        return m_inner->getSnapshotInfo(snapshotName, fileCount, totalSize, timestamp, errorMsg);
    }

private:
    std::unique_ptr<FSProtocol> m_inner;
    LRUCache<std::string, std::string> m_cache;  // 线程安全的LRU缓存
    size_t m_capacity;
    // 注意：m_cacheMutex 已移除，因为 LRUCache 内部已实现线程安全
};

// 引入真实文件系统适配器
#include "../../include/protocol/RealFileSystemAdapter.h"

// 工厂函数：供 ProtocolFactory 使用
std::unique_ptr<FSProtocol> createFSProtocol() {
    // server侧默认启用一个小容量文件内容缓存，以匹配架构设计中的 Cache(LRU)

#if defined(SERVER_USE_REAL_FS) && SERVER_USE_REAL_FS
    // 使用真实的 FileSystem 适配器
    // 磁盘镜像路径：相对于 server 可执行文件的位置
    const std::string diskPath = "../../filesystem/disk/disk.img";

    try {
        auto real = std::make_unique<RealFileSystemAdapter>(diskPath);
        return std::make_unique<CachingFSProtocol>(std::move(real), 64);
    } catch (const std::exception& e) {
        // 静默回退到内存版本，不输出错误信息
        // 内存版本同样支持所有功能，只是数据不持久化
        auto real = std::make_unique<RealFSProtocol>();
        return std::make_unique<CachingFSProtocol>(std::move(real), 64);
    }
#else
    // Windows/MSVC 默认使用内存版本，避免引入 filesystem 源码的 POSIX 依赖
    auto real = std::make_unique<RealFSProtocol>();
    return std::make_unique<CachingFSProtocol>(std::move(real), 64);
#endif
}