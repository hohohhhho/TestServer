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

int main() {
    std::cout << "=== 存储引擎测试开始 ===\n\n";

    // 测试1: 基本操作
    std::cout << "测试1: 基本操作测试\n";
    std::cout << "=" << 50 << "\n";
    test_basic_operations();

    // 测试2: LRU淘汰策略
    std::cout << "\n测试2: LRU淘汰策略测试\n";
    std::cout << "=" << 50 << "\n";
    test_lru_eviction();

    // 测试3: 性能测试
    std::cout << "\n测试3: 性能测试\n";
    std::cout << "=" << 50 << "\n";
    test_performance();

    std::cout << "\n=== 所有测试完成 ===\n";
    return 0;
}

