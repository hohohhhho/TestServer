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

    // ���ڵ��Ƶ�β�������ʹ�ã�
    void move_to_tail(DataNode* node) {
        if (node == tail || !node) return;

        // �ӵ�ǰλ���Ƴ�
        if (node == head) {
            head = head->lru_next;
            if (head) head->lru_prev = nullptr;
        }
        else {
            if (node->lru_prev) node->lru_prev->lru_next = node->lru_next;
            if (node->lru_next) node->lru_next->lru_prev = node->lru_prev;
        }

        // ���ӵ�β��
        if (tail) {
            tail->lru_next = node;
            node->lru_prev = tail;
            node->lru_next = nullptr;
        }
        tail = node;

        if (!head) head = node;
    }

    // �Ƴ�ͷ���ڵ㣨���δʹ�ã�
    DataNode* evict_head() {
        if (!head) return nullptr;

        DataNode* evicted = head;
        node_map.erase(evicted->key);

        head = head->lru_next;
        if (head) {
            head->lru_prev = nullptr;
        }
        else {
            tail = nullptr; // ����Ϊ��
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

    // ���ӻ���½ڵ�
    DataNode* put(const std::string& key, User value) {
        DataNode* evicted = nullptr;

        auto it = node_map.find(key);
        if (it != node_map.end()) {
            // �������нڵ�
            it->second->value = value;
            move_to_tail(it->second);
            return nullptr;
        }

        // �����½ڵ�
        DataNode* new_node = new DataNode(key, value);

        // ���LRU�������������δʹ�õĽڵ�
        if (current_size >= max_size) {
            evicted = evict_head();
        }

        // �����½ڵ㵽β��
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

        return evicted; // ���ر�����Ľڵ㣨��Ҫ�ӹ�ϣ�����Ƴ���
    }

    // ��ȡ�ڵ�
    DataNode* get(const std::string& key) {
        auto it = node_map.find(key);
        if (it != node_map.end()) {
            move_to_tail(it->second);
            return it->second;
        }
        return nullptr;
    }

    // �Ƴ��ڵ�
    DataNode* remove(const std::string& key) {
        auto it = node_map.find(key);
        if (it == node_map.end()) return nullptr;

        DataNode* node = it->second;
        node_map.erase(it);

        // ���������Ƴ�
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

    // ��ղ�ɾ�����нڵ�
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