#include "include/protocol/ProtocolFactory.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

// 客户端处理函数，将在一个独立的线程中运行
void HandleClientConnection(SOCKET clientSocket) {
    std::cout << "Thread " << std::this_thread::get_id() << ": Handling new client on socket " << clientSocket << std::endl;
    ProtocolFactory::handleRequest(clientSocket);
    
    // 优雅地关闭连接
    shutdown(clientSocket, SD_SEND);
    closesocket(clientSocket);
    std::cout << "Thread " << std::this_thread::get_id() << ": Connection closed for socket " << clientSocket << std::endl;
}

class Server {
public:
    ~Server() {
        if (m_listenSocket != INVALID_SOCKET) closesocket(m_listenSocket);
        WSACleanup();
    }

    bool start(int port) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
            return false;
        }

        m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_listenSocket == INVALID_SOCKET) {
            std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
            return false;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (bind(m_listenSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
            return false;
        }

        if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
            return false;
        }

        std::cout << "Server listening on port " << port << "..." << std::endl;

        while (true) {
            SOCKET clientSocket = accept(m_listenSocket, nullptr, nullptr);
            if (clientSocket == INVALID_SOCKET) {
                std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
                continue;
            }
            
            // 为新连接创建一个独立的线程来处理，主线程继续监听
            std::thread clientThread(HandleClientConnection, clientSocket);
            clientThread.detach(); // 分离线程，让它在后台独立运行
        }
        return true;
    }

private:
    SOCKET m_listenSocket = INVALID_SOCKET;
};

int main() {
    Server server;
    if (!server.start(8080)) {
        std::cerr << "Failed to start the server." << std::endl;
        return 1;
    }
    return 0;
}