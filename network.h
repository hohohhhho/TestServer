//network.h
#pragma once
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include <poll.h>
#include <atomic>
#include <cerrno>
#include <unordered_map>
#include <sys/epoll.h>
#include <unistd.h>
#include <atomic>


// 事件类型
enum class EventType {
    READ = 1,
    WRITE = 2,
    ERROR = 4
};

// 事件处理器接口
class EventHandler {
public:
    virtual ~EventHandler() = default;
    
    // 处理事件
    virtual void handle_event(int fd, EventType events) = 0;
    
    // 获取文件描述符
    virtual int get_fd() const = 0;
};

// 连接处理器（不再继承EventHandler）
class ConnectionHandler {
public:
    virtual ~ConnectionHandler() = default;
    
    // 当新连接建立时调用
    virtual void on_connected(int client_fd, const sockaddr_in& addr) = 0;
    
    // 当收到数据时调用
    virtual void on_data(int client_fd, const char* data, size_t len) = 0;
    
    // 当连接关闭时调用
    virtual void on_closed(int client_fd) = 0;
    
    // 发送数据（可选）
    virtual bool send_data(int client_fd, const char* data, size_t len) = 0;
};

// 事件循环接口
class EventLoop {
public:
    virtual ~EventLoop() = default;
    
    // 添加事件监听
    virtual bool add_event(int fd, EventType events, EventHandler* handler) = 0;
    
    // 修改事件
    virtual bool mod_event(int fd, EventType events, EventHandler* handler) = 0;
    
    // 删除事件
    virtual bool del_event(int fd) = 0;
    
    // 运行事件循环
    virtual void run() = 0;
    
    // 停止事件循环
    virtual void stop() = 0;
    
    // 创建事件循环实例
    static std::unique_ptr<EventLoop> create(const std::string& type);
};

class PollLoop : public EventLoop {
private:
    std::vector<pollfd> poll_fds_;
    std::unordered_map<int, EventHandler*> handlers_;
    std::atomic<bool> running_{false};
    
    // 查找fd在poll_fds_中的索引
    int find_fd_index(int fd) {
        for (size_t i = 0; i < poll_fds_.size(); i++) {
            if (poll_fds_[i].fd == fd) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
    
    // 转换为poll事件
    short events_to_poll(EventType events) {
        short result = 0;
        if (static_cast<int>(events) & static_cast<int>(EventType::READ)) {
            result |= POLLIN;
        }
        if (static_cast<int>(events) & static_cast<int>(EventType::WRITE)) {
            result |= POLLOUT;
        }
        if (static_cast<int>(events) & static_cast<int>(EventType::ERROR)) {
            result |= POLLERR;
        }
        return result;
    }
    
    // 转换为事件类型
    EventType poll_to_events(short revents) {
        int events = 0;
        if (revents & POLLIN) events |= static_cast<int>(EventType::READ);
        if (revents & POLLOUT) events |= static_cast<int>(EventType::WRITE);
        if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
            events |= static_cast<int>(EventType::ERROR);
        }
        return static_cast<EventType>(events);
    }
    
public:
    PollLoop() = default;
    ~PollLoop() override = default;
    
    bool add_event(int fd, EventType events, EventHandler* handler) override {
        if (handlers_.find(fd) != handlers_.end()) {
            return false;  // 已存在
        }
        
        pollfd pfd;
        pfd.fd = fd;
        pfd.events = events_to_poll(events);
        pfd.revents = 0;
        
        poll_fds_.push_back(pfd);
        handlers_[fd] = handler;
        return true;
    }
    
    bool mod_event(int fd, EventType events, EventHandler* handler) override {
        auto it = handlers_.find(fd);
        if (it == handlers_.end()) {
            return false;
        }
        
        int index = find_fd_index(fd);
        if (index == -1) {
            return false;
        }
        
        poll_fds_[index].events = events_to_poll(events);
        handlers_[fd] = handler;
        return true;
    }
    
    bool del_event(int fd) override {
        auto it = handlers_.find(fd);
        if (it == handlers_.end()) {
            return false;
        }
        
        int index = find_fd_index(fd);
        if (index != -1) {
            poll_fds_.erase(poll_fds_.begin() + index);
        }
        
        handlers_.erase(fd);
        return true;
    }
    
    void run() override {
        running_ = true;
        
        while (running_) {
            // 调用poll，超时时间1000ms
            int ready = poll(poll_fds_.data(), poll_fds_.size(), 1000);
            
            if (ready < 0) {
                if (errno == EINTR) continue;
                break;  // 发生错误
            }
            
            if (ready == 0) {
                continue;  // 超时
            }
            
            // 处理就绪的事件
            for (size_t i = 0; i < poll_fds_.size(); i++) {
                if (poll_fds_[i].revents != 0) {
                    int fd = poll_fds_[i].fd;
                    EventType events = poll_to_events(poll_fds_[i].revents);
                    
                    auto it = handlers_.find(fd);
                    if (it != handlers_.end()) {
                        it->second->handle_event(fd, events);
                    }
                    
                    if (--ready <= 0) {
                        break;  // 所有就绪事件已处理
                    }
                }
            }
        }
    }
    
    void stop() override {
        running_ = false;
    }
};



class EpollLoop : public EventLoop {
private:
    int epoll_fd_{-1};
    std::unordered_map<int, EventHandler*> handlers_;
    std::atomic<bool> running_{false};
    static const int MAX_EVENTS = 64;
    
    // 转换为epoll事件
    uint32_t events_to_epoll(EventType events) {
        uint32_t result = 0;
        if (static_cast<int>(events) & static_cast<int>(EventType::READ)) {
            result |= EPOLLIN;
        }
        if (static_cast<int>(events) & static_cast<int>(EventType::WRITE)) {
            result |= EPOLLOUT;
        }
        if (static_cast<int>(events) & static_cast<int>(EventType::ERROR)) {
            result |= EPOLLERR;
        }
        return result;
    }
    
    // 转换为事件类型
    EventType epoll_to_events(uint32_t events) {
        int result = 0;
        if (events & EPOLLIN) result |= static_cast<int>(EventType::READ);
        if (events & EPOLLOUT) result |= static_cast<int>(EventType::WRITE);
        if (events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            result |= static_cast<int>(EventType::ERROR);
        }
        return static_cast<EventType>(result);
    }
    
public:
    EpollLoop() {
        epoll_fd_ = epoll_create1(0);
    }
    
    ~EpollLoop() override {
        if (epoll_fd_ != -1) {
            close(epoll_fd_);
        }
    }
    
    bool add_event(int fd, EventType events, EventHandler* handler) override {
        if (handlers_.find(fd) != handlers_.end()) {
            return false;
        }
        
        epoll_event ev;
        ev.events = events_to_epoll(events);
        ev.data.fd = fd;
        
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
            return false;
        }
        
        handlers_[fd] = handler;
        return true;
    }
    
    bool mod_event(int fd, EventType events, EventHandler* handler) override {
        auto it = handlers_.find(fd);
        if (it == handlers_.end()) {
            return false;
        }
        
        epoll_event ev;
        ev.events = events_to_epoll(events);
        ev.data.fd = fd;
        
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
            return false;
        }
        
        handlers_[fd] = handler;
        return true;
    }
    
    bool del_event(int fd) override {
        auto it = handlers_.find(fd);
        if (it == handlers_.end()) {
            return false;
        }
        
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
            return false;
        }
        
        handlers_.erase(fd);
        return true;
    }
    
    void run() override {
        if (epoll_fd_ < 0) {
            return;
        }
        
        running_ = true;
        epoll_event events[MAX_EVENTS];
        
        while (running_) {
            int ready = epoll_wait(epoll_fd_, events, MAX_EVENTS, 1000);
            
            if (ready < 0) {
                if (errno == EINTR) continue;
                break;
            }
            
            for (int i = 0; i < ready; i++) {
                int fd = events[i].data.fd;
                EventType evt = epoll_to_events(events[i].events);
                
                auto it = handlers_.find(fd);
                if (it != handlers_.end()) {
                    it->second->handle_event(fd, evt);
                }
            }
        }
    }
    
    void stop() override {
        running_ = false;
    }
};