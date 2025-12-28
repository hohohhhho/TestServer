// intrusive_lru.hpp
#pragma once
#include "config.h"
#include <unordered_map>

class IntrusiveLRU {
private:
    DataNode* head = nullptr;
    DataNode* tail = nullptr;
    int current_size = 0;
    int max_size;
    std::unordered_map<std::string, DataNode*> node_map;

    // 将节点移到尾部（最近使用）
    void move_to_tail(DataNode* node) {
        if (node == tail || !node) return;

        // 从当前位置移除
        if (node == head) {
            head = head->lru_next;
            if (head) head->lru_prev = nullptr;
        }
        else {
            if (node->lru_prev) node->lru_prev->lru_next = node->lru_next;
            if (node->lru_next) node->lru_next->lru_prev = node->lru_prev;
        }

        // 添加到尾部
        if (tail) {
            tail->lru_next = node;
            node->lru_prev = tail;
            node->lru_next = nullptr;
        }
        tail = node;

        if (!head) head = node;
    }

    // 移除头部节点（最久未使用）
    DataNode* evict_head() {
        if (!head) return nullptr;

        DataNode* evicted = head;
        node_map.erase(evicted->key);

        head = head->lru_next;
        if (head) {
            head->lru_prev = nullptr;
        }
        else {
            tail = nullptr; // 链表为空
        }

        evicted->lru_prev = nullptr;
        evicted->lru_next = nullptr;
        current_size--;

        return evicted;
    }

public:
    IntrusiveLRU(int capacity) : max_size(capacity) {}

    ~IntrusiveLRU() {
        clear();
    }

    // 添加或更新节点
    DataNode* put(const std::string& key, User value) {
        DataNode* evicted = nullptr;

        auto it = node_map.find(key);
        if (it != node_map.end()) {
            // 更新现有节点
            it->second->value = value;
            move_to_tail(it->second);
            return nullptr;
        }

        // 创建新节点
        DataNode* new_node = new DataNode(key, value);

        // 如果LRU已满，驱逐最久未使用的节点
        if (current_size >= max_size) {
            evicted = evict_head();
        }

        // 添加新节点到尾部
        if (!head) {
            head = tail = new_node;
        }
        else {
            tail->lru_next = new_node;
            new_node->lru_prev = tail;
            tail = new_node;
        }

        node_map[key] = new_node;
        current_size++;

        return evicted; // 返回被驱逐的节点（需要从哈希表中移除）
    }

    // 获取节点
    DataNode* get(const std::string& key) {
        auto it = node_map.find(key);
        if (it != node_map.end()) {
            move_to_tail(it->second);
            return it->second;
        }
        return nullptr;
    }

    // 移除节点
    DataNode* remove(const std::string& key) {
        auto it = node_map.find(key);
        if (it == node_map.end()) return nullptr;

        DataNode* node = it->second;
        node_map.erase(it);

        // 从链表中移除
        if (node == head && node == tail) {
            head = tail = nullptr;
        }
        else if (node == head) {
            head = head->lru_next;
            if (head) head->lru_prev = nullptr;
        }
        else if (node == tail) {
            tail = tail->lru_prev;
            if (tail) tail->lru_next = nullptr;
        }
        else {
            if (node->lru_prev) node->lru_prev->lru_next = node->lru_next;
            if (node->lru_next) node->lru_next->lru_prev = node->lru_prev;
        }

        node->lru_prev = nullptr;
        node->lru_next = nullptr;
        current_size--;

        return node;
    }

    // 清空并删除所有节点
    void clear() {
        DataNode* node = head;
        while (node) {
            DataNode* next = node->lru_next;
            delete node;
            node = next;
        }
        head = tail = nullptr;
        node_map.clear();
        current_size = 0;
    }

    int get_size() const { return current_size; }
    int get_capacity() const { return max_size; }
};