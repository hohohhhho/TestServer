// storage_engine.hpp
#pragma once
#include "hash.h"
#include "lru.h"
#include <memory>
#include <mutex>
#include <iostream>

class StorageEngine {
private:
    std::unique_ptr<IntrusiveHashTable> hash_table;
    std::unique_ptr<IntrusiveLRU> lru_cache;
    bool use_lru;
    mutable std::mutex mtx;  // 用于线程安全

public:
    StorageEngine(int hash_capacity = 1024, int lru_capacity = 100, bool enable_lru = true)
        : use_lru(enable_lru) {
        hash_table = std::make_unique<IntrusiveHashTable>(hash_capacity);
        if (use_lru) {
            lru_cache = std::make_unique<IntrusiveLRU>(lru_capacity);
        }
    }

    // 插入或更新键值对
    bool set(const std::string& key, const User& value) {
        std::lock_guard<std::mutex> lock(mtx);

        if (use_lru && lru_cache) {
            DataNode* evicted = lru_cache->put(key, value);

            // 如果LRU驱逐了节点，从哈希表中删除
            if (evicted) {
                DataNode* removed = hash_table->remove(evicted->key);
                if (removed != evicted) {
                    // 应该不会发生，但为了安全
                    delete evicted;
                }
                else {
                    delete evicted;
                }
            }

            // 获取LRU中的节点（可能是新创建的，也可能是已存在的）
            DataNode* node = lru_cache->get(key);
            if (node) {
                // 插入到哈希表
                DataNode* replaced = hash_table->insert(node);
                if (replaced && replaced != node) {
                    // 如果替换了其他节点，删除它
                    delete replaced;
                }
                return true;
            }
        }
        else {
            // 如果不使用LRU，直接创建节点并插入哈希表
            DataNode* node = new DataNode(key, value);
            DataNode* replaced = hash_table->insert(node);
            if (replaced && replaced != node) {
                delete replaced;
            }
            return true;
        }

        return false;
    }

    //// 获取键值对
    //std::pair<bool, User> get(const std::string& key) {
    //    std::lock_guard<std::mutex> lock(mtx);

    //    // 首先在哈希表中查找
    //    DataNode* node = hash_table->find(key);
    //    if (node) {
    //        if (use_lru && lru_cache) {
    //            // 更新LRU中的位置
    //            DataNode* lru_node = lru_cache->get(key);
    //            if (!lru_node) {
    //                // 如果LRU中没有，添加到LRU（可能触发驱逐）
    //                lru_cache->put(key, node->value);
    //            }
    //        }
    //        return { true, node->value };
    //    }

    //    return { false, User(-1) };
    //}
    std::pair<bool, User> get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx);

        // 首先在LRU中查找
        if (use_lru && lru_cache) {
            DataNode* lru_node = lru_cache->get(key);
            if (lru_node) {
                // 在LRU中找到，也一定在哈希表中
                return { true, lru_node->value };
            }
        }

        // 在哈希表中查找
        DataNode* hash_node = hash_table->find(key);
        if (hash_node) {
            if (use_lru && lru_cache) {
                // 从哈希表中取出节点，放入LRU
                DataNode* removed = hash_table->remove(key);
                if (removed) {
                    // 放入LRU（可能会触发淘汰）
                    DataNode* evicted = lru_cache->put(key, removed->value);
                    if (evicted) {
                        // 从哈希表中删除被驱逐的节点
                        hash_table->remove(evicted->key);
                        delete evicted;
                    }
                    // 将节点重新插入哈希表
                    DataNode* replaced = hash_table->insert(removed);
                    if (replaced && replaced != removed) {
                        delete replaced;
                    }
                }
            }
            return { true, hash_node->value };
        }

        return { false, User(-1) };
    }

    // 删除键值对
    bool del(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx);

        // 从哈希表中删除
        DataNode* hash_node = hash_table->remove(key);

        if (use_lru && lru_cache) {
            // 从LRU中删除
            DataNode* lru_node = lru_cache->remove(key);

            // 删除节点
            if (hash_node) {
                delete hash_node;
                return true;
            }
            else if (lru_node) {
                delete lru_node;
                return true;
            }
        }
        else {
            if (hash_node) {
                delete hash_node;
                return true;
            }
        }

        return false;
    }

    // 获取统计信息
    void get_stats() const {
        std::lock_guard<std::mutex> lock(mtx);

        std::cout << "=== 存储引擎统计 ===" << std::endl;
        std::cout << "哈希表容量: " << hash_table->get_capacity() << std::endl;
        std::cout << "哈希表大小: " << hash_table->get_size() << std::endl;
        std::cout << "哈希表负载因子: " << hash_table->get_load_factor() << std::endl;

        if (use_lru && lru_cache) {
            std::cout << "LRU缓存容量: " << lru_cache->get_capacity() << std::endl;
            std::cout << "LRU缓存大小: " << lru_cache->get_size() << std::endl;
        }
    }

    // 清空所有数据
    void clear() {
        std::lock_guard<std::mutex> lock(mtx);

        hash_table->clear();
        if (use_lru && lru_cache) {
            lru_cache->clear();
        }
    }
};

// 测试函数声明
void test_basic_operations();
void test_lru_eviction();
void test_performance();

// 测试1: 基本操作测试
void test_basic_operations() {
    std::cout << "创建存储引擎(哈希表容量=10, LRU容量=5)...\n";
    StorageEngine storage(10, 5, true);

    // 插入数据
    std::cout << "插入5个用户...\n";
    storage.set("user1", User(1, "张三", 1000));
    storage.set("user2", User(2, "李四", 2000));
    storage.set("user3", User(3, "王五", 3000));
    storage.set("user4", User(4, "赵六", 4000));
    storage.set("user5", User(5, "钱七", 5000));

    // 查询数据
    std::cout << "\n查询用户:\n";
    auto result1 = storage.get("user1");
    if (result1.first) {
        std::cout << "  user1: id=" << result1.second.id << ", name=" << result1.second.name
            << ", cash=" << result1.second.cash << " (√)\n";
    }
    else {
        std::cout << "  user1: 未找到 (×)\n";
    }

    auto result2 = storage.get("user2");
    if (result2.first) {
        std::cout << "  user2: id=" << result2.second.id << ", name=" << result2.second.name
            << ", cash=" << result2.second.cash << " (√)\n";
    }
    else {
        std::cout << "  user2: 未找到 (×)\n";
    }

    // 查询不存在的用户
    auto result3 = storage.get("nonexistent");
    if (!result3.first) {
        std::cout << "  nonexistent: 未找到 (√)\n";
    }
    else {
        std::cout << "  nonexistent: 不应该找到 (×)\n";
    }

    // 更新数据
    std::cout << "\n更新user1的余额为1500...\n";
    storage.set("user1", User(1, "张三", 1500));
    auto result4 = storage.get("user1");
    if (result4.first && result4.second.cash == 1500) {
        std::cout << "  user1余额更新成功: " << result4.second.cash << " (√)\n";
    }
    else {
        std::cout << "  user1余额更新失败 (×)\n";
    }

    // 删除数据
    std::cout << "\n删除user2...\n";
    bool deleted = storage.del("user2");
    if (deleted) {
        std::cout << "  user2删除成功 (√)\n";
    }
    else {
        std::cout << "  user2删除失败 (×)\n";
    }

    auto result5 = storage.get("user2");
    if (!result5.first) {
        std::cout << "  user2确认已删除 (√)\n";
    }
    else {
        std::cout << "  user2仍然存在 (×)\n";
    }

    // 显示统计信息
    std::cout << "\n最终统计信息:\n";
    storage.get_stats();

    std::cout << "\n基本操作测试完成！\n";
}

// 测试2: LRU淘汰策略测试
void test_lru_eviction() {
    std::cout << "创建存储引擎(哈希表容量=20, LRU容量=3)...\n";
    StorageEngine storage(20, 3, true);

    // 插入3个用户（LRU容量为3）
    std::cout << "插入3个用户(填满LRU):\n";
    storage.set("A", User(1, "用户A", 100));
    storage.set("B", User(2, "用户B", 200));
    storage.set("C", User(3, "用户C", 300));

    // 查询所有用户
    std::cout << "\n当前缓存中的用户:\n";
    std::vector<std::string> keys = { "A", "B", "C" };
    for (const auto& key : keys) {
        auto result1 = storage.get(key);
        if (result1.first) {
            std::cout << "  " << key << ": 存在 (id=" << result1.second.id << ")\n";
        }
        else {
            std::cout << "  " << key << ": 不存在\n";
        }
    }

    // 访问A，使其成为最近使用的
    std::cout << "\n访问用户A，使其成为最近使用的...\n";
    storage.get("A");

    // 插入第4个用户，应该淘汰最久未使用的B
    std::cout << "插入用户D(会触发LRU淘汰)...\n";
    storage.set("D", User(4, "用户D", 400));

    std::cout << "\n淘汰后状态:\n";
    std::vector<std::string> keys_after = { "A", "B", "C", "D" };
    for (const auto& key : keys_after) {
        auto result1 = storage.get(key);
        if (result1.first) {
            std::cout << "  " << key << ": 存在 (√)\n";
        }
        else {
            std::cout << "  " << key << ": 不存在\n";
        }
    }

    // 预期结果: A, C, D存在，B被淘汰
    auto result1 = storage.get("A");
    auto result2 = storage.get("B");
    auto result3 = storage.get("C");
    auto result4 = storage.get("D");

    if (result1.first && !result2.first && result3.first && result4.first) {
        std::cout << "\n√ LRU淘汰策略正确: B被淘汰，A,C,D保留\n";
    }
    else {
        std::cout << "\n× LRU淘汰策略错误\n";
    }

    std::cout << "\n最终统计信息:\n";
    storage.get_stats();
}

// 测试3: 性能测试
void test_performance() {
    const int NUM_OPERATIONS = 10000;
    const int LRU_CAPACITY = 1000;

    std::cout << "性能测试修正版: " << NUM_OPERATIONS << " 次操作\n";

    // 测试1：先插入，后查询（模拟缓存未命中后命中的情况）
    {
        std::cout << "\n1. 预热后查询（先插入所有数据，再查询）：\n";
        StorageEngine storage(2000, LRU_CAPACITY, true);

        // 插入所有数据
        auto start_insert = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_OPERATIONS; i++) {
            std::string key = "user_" + std::to_string(i);
            storage.set(key, User(i, "测试用户", i * 100));
        }
        auto end_insert = std::chrono::high_resolution_clock::now();
        auto insert_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_insert - start_insert);
        std::cout << "   插入" << NUM_OPERATIONS << "条记录: " << insert_time.count() << "ms\n";

        // 此时LRU中只有最后1000条数据
        // 查询LRU中的数据（最近插入的1000条）
        auto start_query_recent = std::chrono::high_resolution_clock::now();
        int recent_hits = 0;
        for (int i = NUM_OPERATIONS - LRU_CAPACITY; i < NUM_OPERATIONS; i++) {
            std::string key = "user_" + std::to_string(i);
            auto result = storage.get(key);
            if (result.first) recent_hits++;
        }
        auto end_query_recent = std::chrono::high_resolution_clock::now();
        auto query_recent_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_query_recent - start_query_recent);
        std::cout << "   查询最近" << LRU_CAPACITY << "条数据: " << query_recent_time.count() << "ms\n";
        std::cout << "   命中次数: " << recent_hits << "/" << LRU_CAPACITY << "\n";

        // 查询LRU范围外的数据
        auto start_query_old = std::chrono::high_resolution_clock::now();
        int old_hits = 0;
        for (int i = 0; i < LRU_CAPACITY; i++) {
            std::string key = "user_" + std::to_string(i);
            auto result = storage.get(key);
            if (result.first) old_hits++;
        }
        auto end_query_old = std::chrono::high_resolution_clock::now();
        auto query_old_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_query_old - start_query_old);
        std::cout << "   查询前" << LRU_CAPACITY << "条数据: " << query_old_time.count() << "ms\n";
        std::cout << "   命中次数: " << old_hits << "/" << LRU_CAPACITY << "\n";
    }

    // 测试2：边插入边查询（模拟实际使用场景）
    {
        std::cout << "\n2. 边插入边查询（模拟实际场景）：\n";
        StorageEngine storage(2000, LRU_CAPACITY, true);

        int total_hits = 0;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < NUM_OPERATIONS; i++) {
            std::string key = "user_" + std::to_string(i);
            storage.set(key, User(i, "测试用户", i * 100));

            // 随机查询已插入的数据
            if (i > 0 && i % 10 == 0) {
                int random_idx = rand() % i;
                std::string query_key = "user_" + std::to_string(random_idx);
                auto result = storage.get(query_key);
                if (result.first) total_hits++;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "   总操作数: " << NUM_OPERATIONS << " 插入 + " << NUM_OPERATIONS / 10 << " 查询\n";
        std::cout << "   总时间: " << total_time.count() << "ms\n";
        std::cout << "   查询命中次数: " << total_hits << "/" << NUM_OPERATIONS / 10 << "\n";
    }
}