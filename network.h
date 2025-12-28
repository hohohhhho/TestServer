#pragma once
#include "client_connection.hpp"
#include "storage_engine.h"
#include <vector>
#include <map>
#include <poll.h>
#include <unistd.h>
#include <iostream>

class PollServer {
private:
    int listen_fd;
    int port;
    bool running;
    StorageEngine storage;
    ProtocolHandler protocol_handler;

    // poll相关
    std::vector<pollfd> poll_fds;
    std::map<int, ClientConnection*> clients;

    // 添加文件描述符到poll
    void add_to_poll(int fd, short events) {
        pollfd pfd;
        pfd.fd = fd;
        pfd.events = events;
        pfd.revents = 0;
        poll_fds.push_back(pfd);
    }

    // 从poll中移除文件描述符
    void remove_from_poll(int fd) {
        for (auto it = poll_fds.begin(); it != poll_fds.end(); ++it) {
            if (it->fd == fd) {
                poll_fds.erase(it);
                break;
            }
        }
    }

    // 处理新连接
    void handle_new_connection() {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept failed");
            return;
        }

        // 设置非阻塞
        if (!NetworkUtils::set_nonblocking(client_fd)) {
            close(client_fd);
            return;
        }

        // 获取客户端地址
        std::string client_address = NetworkUtils::get_client_address(client_fd);

        // 创建客户端连接
        ClientConnection* client = new ClientConnection(client_fd, client_address, protocol_handler);

        // 添加到poll和clients映射
        add_to_poll(client_fd, POLLIN | POLLOUT);
        clients[client_fd] = client;

        std::cout << "新客户端连接: " << client_address
            << " (FD: " << client_fd << ")" << std::endl;
    }

    // 处理客户端数据
    void handle_client_data(int fd) {
        auto it = clients.find(fd);
        if (it == clients.end()) {
            return;
        }

        ClientConnection* client = it->second;

        // 接收数据
        if (!client->receive()) {
            // 客户端断开连接
            std::cout << "客户端断开连接: " << client->get_address() << std::endl;
            remove_client(fd);
            return;
        }

        // 处理接收到的数据
        client->process_data();
    }

    // 处理客户端发送
    void handle_client_send(int fd) {
        auto it = clients.find(fd);
        if (it == clients.end()) {
            return;
        }

        ClientConnection* client = it->second;

        if (client->has_data_to_send()) {
            if (!client->send()) {
                // 发送失败，断开连接
                std::cout << "发送失败，断开连接: " << client->get_address() << std::endl;
                remove_client(fd);
            }
        }
    }

    // 移除客户端
    void remove_client(int fd) {
        auto it = clients.find(fd);
        if (it != clients.end()) {
            ClientConnection* client = it->second;

            // 从poll中移除
            remove_from_poll(fd);

            // 关闭套接字
            client->close();

            // 删除客户端对象
            delete client;

            // 从映射中移除
            clients.erase(it);
        }
    }

    // 清理资源
    void cleanup() {
        for (auto& [fd, client] : clients) {
            delete client;
        }
        clients.clear();
        poll_fds.clear();

        if (listen_fd != -1) {
            close(listen_fd);
            listen_fd = -1;
        }
    }

public:
    PollServer(int p = DEFAULT_PORT,
        int hash_size = 1024,
        int lru_size = 100)
        : port(p), running(false), listen_fd(-1),
        storage(hash_size, lru_size, lru_size > 0),
        protocol_handler(storage) {}

    ~PollServer() {
        stop();
    }

    // 启动服务器
    bool start() {
        // 创建监听套接字
        listen_fd = NetworkUtils::create_listen_socket(port);
        if (listen_fd < 0) {
            return false;
        }

        std::cout << "服务器启动在端口 " << port << std::endl;
        std::cout << "使用poll网络模型" << std::endl;

        // 添加监听套接字到poll
        add_to_poll(listen_fd, POLLIN);

        running = true;
        event_loop();

        return true;
    }

    // 停止服务器
    void stop() {
        running = false;
        cleanup();
    }

private:
    // 事件循环
    void event_loop() {
        while (running) {
            // 调用poll
            int ready = poll(poll_fds.data(), poll_fds.size(), -1);

            if (ready < 0) {
                if (errno == EINTR) {
                    continue;  // 被信号中断
                }
                perror("poll failed");
                break;
            }

            if (ready == 0) {
                continue;  // 超时
            }

            // 处理就绪的文件描述符
            for (size_t i = 0; i < poll_fds.size(); ++i) {
                if (poll_fds[i].revents == 0) {
                    continue;
                }

                // 检查错误
                if (poll_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    if (poll_fds[i].fd == listen_fd) {
                        std::cerr << "监听套接字错误" << std::endl;
                        running = false;
                        break;
                    }
                    else {
                        // 客户端错误
                        remove_client(poll_fds[i].fd);
                        continue;
                    }
                }

                // 监听套接字可读（新连接）
                if (poll_fds[i].fd == listen_fd) {
                    if (poll_fds[i].revents & POLLIN) {
                        handle_new_connection();
                    }
                }
                // 客户端套接字
                else {
                    int fd = poll_fds[i].fd;

                    // 可读
                    if (poll_fds[i].revents & POLLIN) {
                        handle_client_data(fd);
                    }

                    // 可写
                    if (poll_fds[i].revents & POLLOUT) {
                        handle_client_send(fd);
                    }
                }
            }
        }

        std::cout << "服务器停止" << std::endl;
    }
};