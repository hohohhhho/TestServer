#pragma once
#include <string>

class User {
public:
    User() {
        id = -1;
        name = "未命名";
        cash = 0;
    }
    User(int id, std::string name = "未命名", long long cash = 0) :id(id), name(name), cash(cash) {};
    int id;
    std::string name;
    long long cash;
};

// 统一的侵入式数据节点
class DataNode {
public:
    // 用于哈希表的链表指针
    DataNode* hash_next = nullptr;
    // 用于LRU的双向链表指针
    DataNode* lru_prev = nullptr;
    DataNode* lru_next = nullptr;

    std::string key;     // 键
    User value;          // 值
    int hash_index = -1; // 在哈希表中的位置（用于快速删除）

    DataNode(const std::string& k, const User& v)
        : key(k), value(v) {}

    // 重置指针
    void reset() {
        hash_next = nullptr;
        lru_prev = nullptr;
        lru_next = nullptr;
        hash_index = -1;
    }
};