#pragma once
#include <string>
#include <vector>

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
};