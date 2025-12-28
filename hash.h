#pragma once
#include "config.h"
#include <vector>

class IntrusiveHashTable {
private:
    std::vector<DataNode*> buckets;
    int capacity;
    int size;
    double load_factor = 0.75;

    // ��ϣ����
    unsigned int hash(const std::string& key) {
        unsigned int hash = 5381;
        for (char c : key) {
            hash = ((hash << 5) + hash) + c; // hash * 33 + c
        }
        return hash % capacity;
    }

public:
    IntrusiveHashTable(int cap = 16) : capacity(cap), size(0) {
        buckets.resize(capacity, nullptr);
    }

    ~IntrusiveHashTable() {
        // ע�⣺���ﲻɾ���ڵ㣬��LRUͳһ����
        clear();
    }

    // ����ڵ㣨���ر��滻�ľɽڵ㣬���û���򷵻�nullptr��
    DataNode* insert(DataNode* node) {
        if (!node) return nullptr;

        unsigned int index = hash(node->key);
        node->hash_index = index;

        DataNode* current = buckets[index];
        DataNode* prev = nullptr;

        // �����Ƿ��Ѵ�����ͬkey
        while (current) {
            if (current->key == node->key) {
                // �滻���нڵ�
                node->hash_next = current->hash_next;
                if (prev) {
                    prev->hash_next = node;
                }
                else {
                    buckets[index] = node;
                }
                return current; // ���ر��滻�Ľڵ�
            }
            prev = current;
            current = current->hash_next;
        }

        // ���뵽����ͷ��
        node->hash_next = buckets[index];
        buckets[index] = node;
        size++;

        // ����Ƿ���Ҫ����
        if (size > capacity * load_factor) {
            resize(capacity * 2);
        }

        return nullptr; // û���滻�κνڵ�
    }

    // ���ҽڵ�
    DataNode* find(const std::string& key) {
        unsigned int index = hash(key);
        DataNode* node = buckets[index];

        while (node) {
            if (node->key == key) {
                return node;
            }
            node = node->hash_next;
        }
        return nullptr;
    }

    // ɾ���ڵ�
    DataNode* remove(const std::string& key) {
        unsigned int index = hash(key);
        DataNode* node = buckets[index];
        DataNode* prev = nullptr;

        while (node) {
            if (node->key == key) {
                if (prev) {
                    prev->hash_next = node->hash_next;
                }
                else {
                    buckets[index] = node->hash_next;
                }
                node->hash_next = nullptr;
                node->hash_index = -1;
                size--;
                return node;
            }
            prev = node;
            node = node->hash_next;
        }
        return nullptr;
    }

    // ����
    void resize(int new_capacity) {
        std::vector<DataNode*> new_buckets(new_capacity, nullptr);
        int old_capacity = capacity;
        capacity = new_capacity;

        for (int i = 0; i < old_capacity; i++) {
            DataNode* node = buckets[i];
            while (node) {
                DataNode* next = node->hash_next;
                unsigned int new_index = hash(node->key);
                node->hash_index = new_index;

                // ���뵽��Ͱ
                node->hash_next = new_buckets[new_index];
                new_buckets[new_index] = node;

                node = next;
            }
        }

        buckets = std::move(new_buckets);
    }

    // ��գ���ɾ���ڵ㣩
    void clear() {
        for (auto& bucket : buckets) {
            bucket = nullptr;
        }
        size = 0;
    }

    int get_size() const { return size; }
    int get_capacity() const { return capacity; }
    double get_load_factor() const { return (double)size / capacity; }
};