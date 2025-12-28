//server.h
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <atomic>

class NetworkManager {
private:
    int server_fd = -1;
    std::atomic<bool> running{false};
    
public:
    bool init(int port = 8899) {
        // 创建socket
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == -1) {
            std::cerr << "创建socket失败" << std::endl;
            return false;
        }

        // 设置端口复用，避免"Address already in use"错误
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // 绑定地址和端口
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "绑定端口失败" << std::endl;
            close(server_fd);
            return false;
        }

        // 开始监听
        if (listen(server_fd, 5) < 0) {
            std::cerr << "监听失败" << std::endl;
            close(server_fd);
            return false;
        }

        std::cout << "服务器启动，监听端口 " << port << " ..." << std::endl;
        return true;
    }

    void start() {
        running = true;
        
        while (running) {
            std::cout << "等待客户端连接..." << std::endl;
            
            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            
            // 这里会阻塞，直到有客户端连接
            int client_socket = accept(server_fd, (sockaddr*)&client_addr, &addr_len);
            
            if (client_socket < 0) {
                std::cerr << "接受连接失败" << std::endl;
                continue;
            }

            std::cout << "客户端已连接！" << std::endl;
            
            // 为每个客户端创建新线程处理
            std::thread client_thread(&NetworkManager::handle_client, this, client_socket);
            client_thread.detach();  // 分离线程，自动回收资源
        }
    }
    
private:
    void handle_client(int client_socket) {
        char buffer[1024];
        
        while (true) {
            // 清空缓冲区
            memset(buffer, 0, sizeof(buffer));
            
            // 接收数据
            int bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
            
            if (bytes_read <= 0) {
                std::cout << "客户端断开连接" << std::endl;
                break;
            }
            
            std::cout << "收到客户端消息: " << buffer << std::endl;
            
            // 发送响应
            std::string response = "服务器收到: " + std::string(buffer);
            send(client_socket, response.c_str(), response.length(), 0);
        }
        
        close(client_socket);
    }
    
public:
    void stop() {
        running = false;
        if (server_fd != -1) {
            close(server_fd);
            server_fd = -1;
        }
    }
    
    ~NetworkManager() {
        stop();
    }
};