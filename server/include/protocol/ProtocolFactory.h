#pragma once
#include <winsock2.h> // For SOCKET type

// 【协议工厂】
// 职责：根据客户端请求，创建并调度相应的协议处理器
class ProtocolFactory {
public:
    // 静态方法：处理单个客户端连接的完整请求-响应周期
    static void handleRequest(SOCKET clientSocket);
};