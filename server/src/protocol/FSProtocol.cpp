#include "../../include/protocol/FSProtocol.h"
#include <memory>
#include <iostream>// 需要包含 <memory> 以使用 std::unique_ptr 和 std::make_unique

// 具体的文件系统协议实现类
class RealFSProtocol : public FSProtocol {
public:
    bool createSnapshot(const std::string& path, const std::string& snapshotName, std::string& errorMsg) override {
        std::cout << "FSProtocol: createSnapshot called" << std::endl;
        errorMsg = "Not implemented (requires FileSystem DLL)";
        return false;
    }

    bool restoreSnapshot(const std::string& snapshotName, std::string& errorMsg) override {
        std::cout << "FSProtocol: restoreSnapshot called" << std::endl;
        errorMsg = "Not implemented";
        return false;
    }

    std::vector<std::string> listSnapshots(const std::string& path, std::string& errorMsg) override {
        std::cout << "FSProtocol: listSnapshots called" << std::endl;
        errorMsg = "Not implemented";
        return {};
    }

    bool readFile(const std::string& path, std::string& content, std::string& errorMsg) override {
        std::cout << "FSProtocol: readFile called" << std::endl;
        errorMsg = "Not implemented";
        return false;
    }

    bool writeFile(const std::string& path, const std::string& content, std::string& errorMsg) override {
        std::cout << "FSProtocol: writeFile called" << std::endl;
        errorMsg = "Not implemented";
        return false;
    }

    bool deleteFile(const std::string& path, std::string& errorMsg) override {
        std::cout << "FSProtocol: deleteFile called" << std::endl;
        errorMsg = "Not implemented";
        return false;
    }

    bool createDirectory(const std::string& path, std::string& errorMsg) override {
        std::cout << "FSProtocol: createDirectory called" << std::endl;
        errorMsg = "Not implemented";
        return false;
    }

    std::string getFilePermission(const std::string& path, const std::string& user, std::string& errorMsg) override {
        std::cout << "FSProtocol: getFilePermission called" << std::endl;
        errorMsg = "Not implemented";
        return "";
    }

    std::string submitForReview(const std::string& operation, const std::string& path, 
                                       const std::string& user, std::string& errorMsg) override {
        std::cout << "FSProtocol: submitForReview called" << std::endl;
        errorMsg = "Not implemented";
        return "";
    }
};

// 工厂函数：供 ProtocolFactory 使用
std::unique_ptr<FSProtocol> createFSProtocol() {
    return std::make_unique<RealFSProtocol>();
}