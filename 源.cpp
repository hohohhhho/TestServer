#include "lru.h"
#include "hash.h"
#include <iostream>

#include "storage_engine.h"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <cassert>
#include <utility>
#include "server.h"

// src/main.cpp
#include "network.h"
#include "client.h"
#include <iostream>
#include <cstring>
#include <csignal>
#include <sys/socket.h>

class EchoHandler : public ConnectionHandler {
private:
    NetworkServer* server_;
    
public:
    EchoHandler(NetworkServer* server = nullptr) : server_(server) {}
    
    void on_connected(int client_fd, const sockaddr_in& addr) override {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
        std::cout << "[连接] fd=" << client_fd 
                  << ", IP=" << ip 
                  << ", 端口=" << ntohs(addr.sin_port) << std::endl;
        
        std::string welcome = "欢迎连接到Echo服务器！发送任何消息，我会回传给你。\n";
        if (server_) {
            server_->send(client_fd, welcome);
        }
    }
    
    void on_data(int client_fd, const char* data, size_t len) override {
        std::string message(data, len);
        std::cout << "[收到] fd=" << client_fd 
                  << ", 数据: " << message;
        
        // Echo: 将收到的数据发回
        if (server_) {
            server_->send(client_fd, "Echo: " + message);
        }
    }
    
    void on_closed(int client_fd) override {
        std::cout << "[断开] fd=" << client_fd << std::endl;
    }
    
    bool send_data(int client_fd, const char* data, size_t len){
        if (server_) {
            return server_->send(client_fd, data, len);
        }
        return false;
    }
    
    void set_server(NetworkServer* server) {
        server_ = server;
    }
};

// 全局变量用于信号处理
std::atomic<bool> running{true};

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        std::cout << "\n收到停止信号，正在关闭服务器..." << std::endl;
        running = false;
    }
}

int main(int argc, char* argv[]) {
    // 解析命令行参数
    std::string event_loop_type = "poll";  // 默认使用poll
    std::string host = "0.0.0.0";          // 默认监听所有接口
    int port = 8899;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
            event_loop_type = argv[++i];
        } else if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            std::cout << "用法: " << argv[0] << " [选项]\n"
                      << "选项:\n"
                      << "  --model TYPE   事件循环模型 (poll 或 epoll，默认: poll)\n"
                      << "  --host HOST    监听地址 (默认: 0.0.0.0)\n"
                      << "  --port PORT    监听端口 (默认: 8899)\n"
                      << "  --help         显示帮助信息\n"
                      << "\n示例:\n"
                      << "  " << argv[0] << " --model poll --port 8899\n"
                      << "  " << argv[0] << " --model epoll --host 127.0.0.1 --port 8899\n";
            return 0;
        }
    }
    
    // 检查模型类型
    if (event_loop_type != "poll" && event_loop_type != "epoll") {
        std::cerr << "错误: 不支持的事件模型 '" << event_loop_type 
                  << "'，请使用 'poll' 或 'epoll'" << std::endl;
        return 1;
    }
    
    // 注册信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        std::cout << "启动服务器..." << std::endl;
        std::cout << "模型: " << event_loop_type << std::endl;
        std::cout << "地址: " << host << std::endl;
        std::cout << "端口: " << port << std::endl;
        
        // 创建事件循环
        auto loop = EventLoop::create(event_loop_type);
        
        // 创建处理器
        auto handler = std::make_unique<EchoHandler>();
        
        // 创建服务器
        auto server = std::make_unique<NetworkServer>(
            std::move(loop),
            std::move(handler)
        );
        
        // 设置处理器中的服务器指针
        static_cast<EchoHandler*>(server->conn_handler_.get())->set_server(server.get());
        
        // 启动服务器
        if (!server->start(host, port)) {
            std::cerr << "启动服务器失败" << std::endl;
            return 1;
        }
        
        std::cout << "服务器已启动，按Ctrl+C停止..." << std::endl;
        
        // 运行服务器
        server->run();
        
        std::cout << "服务器已停止" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}