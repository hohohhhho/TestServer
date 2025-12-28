// command.h
#pragma once
#include "network.h"
#include "storage_engine.h"
#include "client.h"
#include <sstream>
#include <iostream>
#include <algorithm>

class CommandHandler : public ConnectionHandler {
private:
    NetworkServer* server_;
    StorageEngine& storage_engine_;

    // 分割字符串
    std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(str);
        while (std::getline(tokenStream, token, delimiter)) {
            if (!token.empty()) {
                tokens.push_back(token);
            }
        }
        return tokens;
    }

    // 清理字符串（移除换行符和空格）
    void trim(std::string& s) {
        s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
        s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
        // 移除首尾空格
        s.erase(0, s.find_first_not_of(' '));
        s.erase(s.find_last_not_of(' ') + 1);
    }

    // 处理GET命令
    void handle_get(int client_fd, const std::string& key) {
        std::cout << "[GET] fd=" << client_fd << ", key=" << key << std::endl;

        auto result = storage_engine_.get(key);

        if (result.first) {
            std::stringstream ss;
            ss << "data/"
                << result.second.id << "/"
                << result.second.name << "/"
                << result.second.email << "/"
                << result.second.phone << "/"
                << result.second.cash << "\n";

            server_->send(client_fd, ss.str());
            std::cout << "[GET] 成功找到用户: id=" << result.second.id
                << ", name=" << result.second.name << std::endl;
        }
        else {
            server_->send(client_fd, "fail\n");
            std::cout << "[GET] 未找到用户: key=" << key << std::endl;
        }
    }

    // 处理SET命令
    void handle_set(int client_fd, const std::string& field,
        const std::string& key, const std::string& value) {
        std::cout << "[SET] fd=" << client_fd << ", field=" << field
            << ", key=" << key << ", value=" << value << std::endl;

        auto result = storage_engine_.get(key);

        if (!result.first) {
            // 如果用户不存在，根据key的类型创建新用户
            if (isdigit(key[0])) {
                // key是数字，作为id
                try {
                    int id = std::stoi(key);
                    result.second = User(id, "管理员");
                }
                catch (const std::exception& e) {
                    std::cout << "[SET] 无效的ID: " << key << std::endl;
                    server_->send(client_fd, "fail: 无效的ID\n");
                    return;
                }
            }
            else {
                // key是字符串，作为name
                result.second = User(-1, key);
            }
        }

        // 根据field更新用户信息
        if (field == "name") {
            result.second.name = value;
        }
        else if (field == "email") {
            result.second.email = value;
        }
        else if (field == "phone") {
            result.second.phone = value;
        }
        else if (field == "cash") {
            try {
                long long cash_change = std::stoll(value);
                result.second.cash = cash_change;  // 设置金额
            }
            catch (const std::exception& e) {
                std::cout << "[SET] 无效的金额: " << value << std::endl;
                server_->send(client_fd, "fail: 无效的金额\n");
                return;
            }
        }
        else {
            std::cout << "[SET] 无效的字段: " << field << std::endl;
            server_->send(client_fd, "fail: 无效的字段\n");
            return;
        }

        // 保存到存储引擎
        bool success = storage_engine_.set(key, result.second);

        if (success) {
            server_->send(client_fd, "ok\n");
            std::cout << "[SET] 成功更新: key=" << key
                << ", field=" << field << std::endl;
        }
        else {
            server_->send(client_fd, "fail: 存储失败\n");
            std::cout << "[SET] 存储失败: key=" << key << std::endl;
        }
    }

    // 处理命令
    void process_command(int client_fd, const std::string& command) {
        auto tokens = split(command, '/');

        if (tokens.size() < 2) {
            std::cout << "[错误] 无效的命令格式: " << command << std::endl;
            server_->send(client_fd, "error: 无效的命令格式\n");
            return;
        }

        const std::string& cmd = tokens[0];

        if (cmd == "get" && tokens.size() == 2) {
            std::string key = tokens[1];
            trim(key);
            handle_get(client_fd, key);
        }
        else if (cmd == "set" && tokens.size() == 4) {
            std::string field = tokens[1];
            std::string key = tokens[2];
            std::string value = tokens[3];

            trim(field);
            trim(key);
            trim(value);

            handle_set(client_fd, field, key, value);
        }
        else {
            std::cout << "[错误] 未知命令或参数错误: " << command << std::endl;
            std::stringstream help_msg;
            help_msg << "error: 未知命令或参数错误\n"
                << "可用命令:\n"
                << "  get/<id或name>              - 获取用户信息\n"
                << "  set/<field>/<id或name>/<value> - 设置用户信息\n"
                << "字段(field)支持: name, email, phone, cash\n"
                << "cash字段支持负数表示取款\n";
            server_->send(client_fd, help_msg.str());
        }
    }

public:
    CommandHandler(NetworkServer* server, StorageEngine& storage)
        : server_(server), storage_engine_(storage) {}

    void on_connected(int client_fd, const sockaddr_in& addr) override {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
        std::cout << "[连接] fd=" << client_fd
            << ", IP=" << ip
            << ", 端口=" << ntohs(addr.sin_port) << std::endl;

        std::string welcome =
            "欢迎连接到用户信息存储服务器！\n"
            "可用命令:\n"
            "  get/<id或name>                     - 获取用户信息\n"
            "  set/<field>/<id或name>/<value>     - 设置用户信息\n"
            "字段(field)支持: name, email, phone, cash\n"
            "cash字段支持负数表示取款\n"
            "示例:\n"
            "  get/1001                    - 获取ID为1001的用户信息\n"
            "  get/john                    - 获取姓名为john的用户信息\n"
            "  set/name/john/John Doe      - 设置john的姓名为John Doe\n"
            "  set/cash/1001/1000          - 为用户1001增加1000元\n"
            "  set/cash/1001/-500          - 从用户1001账户取走500元\n\n";

        if (server_) {
            server_->send(client_fd, welcome);
        }
    }

    void on_data(int client_fd, const char* data, size_t len) override {
        std::string command(data, len);

        // 移除首尾空白字符
        trim(command);

        if (command.empty()) {
            return;
        }

        std::cout << "[命令] fd=" << client_fd
            << ", 命令: " << command << std::endl;

        // 处理命令
        process_command(client_fd, command);
    }

    void on_closed(int client_fd) override {
        std::cout << "[断开] fd=" << client_fd << std::endl;
    }

    bool send_data(int client_fd, const char* data, size_t len) override {
        if (server_) {
            return server_->send(client_fd, data, len);
        }
        return false;
    }

    void set_server(NetworkServer* server) {
        server_ = server;
    }
};