#pragma once
#include "network.h"
#include <stdexcept>

std::unique_ptr<EventLoop> EventLoop::create(const std::string& type) {
    if (type == "poll") {
        return std::make_unique<PollLoop>();
    } else if (type == "epoll") {
        return std::make_unique<EpollLoop>();
    } else {
        throw std::runtime_error("Unknown event loop type: " + type);
    }
}

#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <netinet/in.h>
#include <fcntl.h>
#include <iostream>
#include <arpa/inet.h>

// TCP服务器
class NetworkServer {
public:
    std::unique_ptr<EventLoop> event_loop_;
    std::unique_ptr<ConnectionHandler> conn_handler_;
    int server_fd_{-1};
    
    // 内部事件处理器
    class ServerEventHandler : public EventHandler {
    private:
        NetworkServer* server_;
        int fd_;
        
    public:
        ServerEventHandler(NetworkServer* server, int fd) 
            : server_(server), fd_(fd) {}
        
        int get_fd() const override { return fd_; }
        
        void handle_event(int fd, EventType events) override {
            server_->handle_server_event(fd, events);
        }
    };
    
    class ClientEventHandler : public EventHandler {
    private:
        NetworkServer* server_;
        int fd_;
        
    public:
        ClientEventHandler(NetworkServer* server, int fd) 
            : server_(server), fd_(fd) {}
        
        int get_fd() const override { return fd_; }
        
        void handle_event(int fd, EventType events) override {
            server_->handle_client_event(fd, events);
        }
    };
    
    std::unique_ptr<ServerEventHandler> server_handler_;
    std::unordered_map<int, std::unique_ptr<ClientEventHandler>> client_handlers_;
    
    void handle_server_event(int fd, EventType events) {
        if (fd != server_fd_) return;
        
        if (static_cast<int>(events) & static_cast<int>(EventType::READ)) {
            accept_connection();
        }
    }
    
    void handle_client_event(int fd, EventType events) {
        if (static_cast<int>(events) & static_cast<int>(EventType::READ)) {
            char buffer[4096];
            ssize_t n = read(fd, buffer, sizeof(buffer) - 1);
            
            if (n > 0) {
                buffer[n] = '\0';
                conn_handler_->on_data(fd, buffer, n);
            } else {
                // 连接关闭
                conn_handler_->on_closed(fd);
                close_connection(fd);
            }
        }
        
        if (static_cast<int>(events) & static_cast<int>(EventType::ERROR)) {
            conn_handler_->on_closed(fd);
            close_connection(fd);
        }
    }
    
    void accept_connection() {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd_, 
                              reinterpret_cast<sockaddr*>(&client_addr), 
                              &addr_len);
        
        if (client_fd < 0) {
            perror("accept失败");
            return;
        }
        
        // 设置非阻塞
        fcntl(client_fd, F_SETFL, O_NONBLOCK);
        
        // 创建客户端事件处理器
        auto handler = std::make_unique<ClientEventHandler>(this, client_fd);
        
        // 添加到事件循环
        if (event_loop_->add_event(client_fd, EventType::READ, handler.get())) {
            client_handlers_[client_fd] = std::move(handler);
            conn_handler_->on_connected(client_fd, client_addr);
        } else {
            close(client_fd);
        }
    }
    
    void close_connection(int fd) {
        event_loop_->del_event(fd);
        client_handlers_.erase(fd);
        close(fd);
    }
    
public:
    NetworkServer(std::unique_ptr<EventLoop> loop, 
                  std::unique_ptr<ConnectionHandler> handler)
        : event_loop_(std::move(loop))
        , conn_handler_(std::move(handler)) {}
    
    ~NetworkServer() {
        stop();
    }
    
    bool start(const std::string& host, int port) {
        // 创建socket
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd_ < 0) {
            perror("创建socket失败");
            return false;
        }
        
        // 设置地址重用
        int opt = 1;
        if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("设置SO_REUSEADDR失败");
            close(server_fd_);
            return false;
        }
        
        // 绑定地址
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        
        if (host == "0.0.0.0" || host == "") {
            addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
                std::cerr << "无效的IP地址: " << host << std::endl;
                close(server_fd_);
                return false;
            }
        }
        
        if (bind(server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            perror("绑定地址失败");
            close(server_fd_);
            return false;
        }
        
        // 监听
        if (listen(server_fd_, 128) < 0) {
            perror("监听失败");
            close(server_fd_);
            return false;
        }
        
        // 设置非阻塞
        fcntl(server_fd_, F_SETFL, O_NONBLOCK);
        
        // 创建服务器事件处理器
        server_handler_ = std::make_unique<ServerEventHandler>(this, server_fd_);
        
        // 添加到事件循环
        if (!event_loop_->add_event(server_fd_, EventType::READ, server_handler_.get())) {
            std::cerr << "添加服务器事件失败" << std::endl;
            close(server_fd_);
            return false;
        }
        
        std::cout << "服务器启动在 " 
                  << (host.empty() ? "0.0.0.0" : host) 
                  << ":" << port 
                  << "，使用事件模型: " 
                  << (dynamic_cast<PollLoop*>(event_loop_.get()) ? "poll" : "epoll")
                  << std::endl;
        
        return true;
    }
    
    void run() {
        if (server_fd_ >= 0) {
            event_loop_->run();
        }
    }
    
    void stop() {
        if (event_loop_) {
            event_loop_->stop();
        }
        
        for (auto& pair : client_handlers_) {
            close(pair.first);
        }
        client_handlers_.clear();
        
        if (server_fd_ >= 0) {
            close(server_fd_);
            server_fd_ = -1;
        }
    }
    
    bool send(int client_fd, const char* data, size_t len) {
        ssize_t n = write(client_fd, data, len);
        return n == static_cast<ssize_t>(len);
    }
    
    bool send(int client_fd, const std::string& data) {
        return send(client_fd, data.c_str(), data.length());
    }
    
    void disconnect(int client_fd) {
        close_connection(client_fd);
    }
    
    int get_server_fd() const { return server_fd_; }
};