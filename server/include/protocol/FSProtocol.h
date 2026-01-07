#pragma once
#include <string>
#include <vector>

// 文件系统统计信息结构
struct FileSystemStats {
    int block_size;         // 块大小 (字节)
    int block_count;        // 总块数
    int inode_count;        // 总 inode 数
    int free_inode_count;   // 空闲 inode 数
    int free_block_count;   // 空闲块数
    int data_block_start;   // 数据块起始位置
    int snapshot_count;     // 当前快照数量
    bool is_real_fs;        // 是否是真实文件系统（非模拟）
};

// 文件详细信息结构
struct FileInfo {
    std::string path;       // 文件路径
    int link_count;         // 硬链接计数
    int open_count;         // 打开计数
    size_t size;            // 文件大小
    bool is_directory;      // 是否为目录
};

// 【关键】标注所有需 FileSystem 提供的 API
// 注意：此头文件不包含任何 FS 实现，仅定义接口
class FSProtocol {
public:
    virtual ~FSProtocol() = default;

    // 【FileSystem API 调用点 1】创建快照
    virtual bool createSnapshot(const std::string& path, const std::string& snapshotName, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 2】恢复快照
    virtual bool restoreSnapshot(const std::string& snapshotName, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 3】列出快照
    virtual std::vector<std::string> listSnapshots(const std::string& path, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 3.1】获取快照详细信息
    virtual bool getSnapshotInfo(const std::string& snapshotName, int& fileCount, size_t& totalSize, std::string& timestamp, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 4】读取文件
    virtual bool readFile(const std::string& path, std::string& content, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 5】写入文件
    virtual bool writeFile(const std::string& path, const std::string& content, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 6】删除文件
    virtual bool deleteFile(const std::string& path, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 7】创建目录
    virtual bool createDirectory(const std::string& path, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 8】获取文件权限
    virtual std::string getFilePermission(const std::string& path, const std::string& user, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 9】提交审核请求
    virtual std::string submitForReview(const std::string& operation, const std::string& path, 
                                       const std::string& user, std::string& errorMsg) = 0;

    // 【新增】目录遍历/类型查询（用于 CLI: LS/TREE）
    // entries 返回“当前目录下的直接子项名字”，目录项建议以 '/' 结尾表示目录。
    virtual bool listDirectory(const std::string& path, std::vector<std::string>& entries, std::string& errorMsg) = 0;
    // 判断 path 是否为目录；若 path 不存在或读取失败，返回 false 并填写 errorMsg。
    virtual bool isDirectory(const std::string& path, bool& isDirOut, std::string& errorMsg) = 0;
    // 【新增】获取文件系统统计信息
    virtual bool getFileSystemStats(FileSystemStats& stats, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 10】为审核请求分配审稿人
    virtual bool assignReviewer(const std::string& reviewId, const std::string& reviewer, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 11】创建硬链接
    virtual bool createHardLink(const std::string& sourcePath, const std::string& linkPath, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 12】获取文件信息（包括链接计数、打开计数）
    virtual bool getFileInfo(const std::string& path, FileInfo& info, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 13】打开文件（增加打开计数）
    virtual bool openFile(const std::string& path, std::string& errorMsg) = 0;
    
    // 【FileSystem API 调用点 14】关闭文件（减少打开计数）
    virtual bool closeFile(const std::string& path, std::string& errorMsg) = 0;
};