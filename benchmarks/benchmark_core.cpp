/**
 * @file benchmark_core.cpp
 * @brief Kiểm tra hiệu năng Phase 5: Batch Insert, Mmap Caching, và Traversal.
 */

#include <graphlite/graphlite.h>
#include <graphlite/traversal.h>
#include <iostream>
#include <chrono>
#include <vector>
#include <string>

using namespace graphlite;
using namespace graphlite::utils;

int main() {
    std::cout << "==========================================\n";
    std::cout << "    GraphLite v0.2 - Performance Test     \n";
    std::cout << "==========================================\n";

    const char* db_dir = "/tmp/graphlite_bench";
    
    // Khởi tạo Database
    GraphDB db(db_dir);
    db.clearGraph(); // Đảm bảo database sạch sẽ
    
    EdgeType follow_type = db.defineEdgeType("FOLLOWS");

    const int NUM_NODES = 500000;
    const int NUM_EDGES = 4000000;

    std::cout << "[1] Generating " << NUM_NODES << " nodes in memory...\n";
    auto t1 = std::chrono::high_resolution_clock::now();
    
    // Chúng ta tạo trước mảng edges in-memory để loại bỏ overhead sinh số ngẫu nhiên
    MiniVector<EdgeInsertData> edges(NUM_EDGES);
    
    for (int i = 0; i < NUM_EDGES; ++i) {
        edges[i].from = (i % NUM_NODES) + 1;
        edges[i].to = ((i * 3) % NUM_NODES) + 1;
        edges[i].type = follow_type;
        edges[i].payload_size = 0;
    }

    auto t2 = std::chrono::high_resolution_clock::now();
    std::cout << "    -> Done in " 
              << std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count() 
              << " ms\n";

    std::cout << "[2] Executing insertEdgesBatch (" << NUM_EDGES << " edges)...\n";
    auto t3 = std::chrono::high_resolution_clock::now();
    
    db.insertEdgesBatch(edges);
    
    auto t4 = std::chrono::high_resolution_clock::now();
    double insert_time = std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count() / 1000.0;
    std::cout << "    -> Batch Insertion Time: " << insert_time << " seconds\n";
    std::cout << "    -> Throughput: " << (NUM_EDGES / insert_time) << " edges/sec\n";

    if (insert_time < 10.0) {
        std::cout << "    ✅ PASSED: Trúng chỉ tiêu (< 10s)\n";
    } else {
        std::cout << "    ❌ FAILED: Vượt chỉ tiêu (> 10s)\n";
    }

    std::cout << "[3] Testing Cache & Aggregation (CountNeighborsIf)...\n";
    auto t5 = std::chrono::high_resolution_clock::now();
    
    size_t count = CountNeighborsIf(db, 1, follow_type, true);
    
    auto t6 = std::chrono::high_resolution_clock::now();
    double query_time_us = std::chrono::duration_cast<std::chrono::microseconds>(t6 - t5).count();
    std::cout << "    -> Found " << count << " neighbors for Node 1 in " << query_time_us << " us\n";

    std::cout << "[4] Memory Usage Report...\n";
    double mb = db.getMemoryUsage() / (1024.0 * 1024.0);
    std::cout << "    -> Total DB Size: " << mb << " MB\n";

    std::cout << "==========================================\n";
    return 0;
}
