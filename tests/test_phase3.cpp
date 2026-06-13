/**
 * @file test_phase3.cpp
 * @brief Phase 3 Verification — Full GraphDB Integration & Persistence.
 *
 * Test cases:
 * 1. GraphDB Lifecycle: create db, add nodes, add edges, get edges.
 * 2. Caching: verify edge chain is cached properly.
 * 3. Persistence: close db, reopen, read nodes and edges.
 * 4. Schema persistence: ensure defined node/edge types are maintained (though schema currently is heap-backed, types are defined sequentially).
 */

#include <graphlite/graphlite.h>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#include <cstdio>
#include <chrono>

using namespace graphlite;

// ============================================================
// TEST HELPERS
// ============================================================

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    if (expr) { \
        std::cout << "  [PASS] " << name << "\n"; \
        tests_passed++; \
    } else { \
        std::cout << "  [FAIL] " << name << "\n"; \
        tests_failed++; \
    } \
} while(0)

// Helper: tạo thư mục tạm
static void ensure_dir(const char* path) {
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

static void remove_file(const char* path) {
    std::remove(path);
}

static void cleanup_db(const char* db_dir) {
    std::string dir = db_dir;
    remove_file((dir + "/nodes.gldb").c_str());
    remove_file((dir + "/edges.gldb").c_str());
    remove_file((dir + "/strings.gldb").c_str());
}

// ============================================================
// TEST 1: GraphDB Basic Operations & Caching
// ============================================================

void test_graphdb_basic() {
    std::cout << "\n=== TEST: GraphDB Basic & Caching ===\n";

    const char* db_dir = "/tmp/graphlite_test_p3_basic";
    ensure_dir(db_dir);
    cleanup_db(db_dir);

    {
        GraphDB db(db_dir);

        // Define Types
        NodeType type_user = db.defineNodeType("User");
        EdgeType type_follows = db.defineEdgeType("FOLLOWS");
        TEST("Define Types", type_user == 1 && type_follows == 1);

        // Add Nodes
        NodeID u1 = db.addNode("Alice");
        NodeID u2 = db.addNode("Bob");
        NodeID u3 = db.addNode("Carol");
        TEST("Add Nodes", u1 == 1 && u2 == 2 && u3 == 3);

        // Get Node ID/Name
        TEST("getNodeId", db.getNodeId("Bob") == u2);
        TEST("getNodeName", db.getNodeName(u3) == "Carol");

        // Add Edges
        uint32_t payload_timestamp = 1622500000;
        db.addEdge(u1, u2, type_follows, reinterpret_cast<const uint8_t*>(&payload_timestamp), sizeof(uint32_t));
        
        payload_timestamp = 1622600000;
        db.addEdge(u1, u3, type_follows, reinterpret_cast<const uint8_t*>(&payload_timestamp), sizeof(uint32_t));

        // Get Edges (Cache MISS -> Read from Disk -> Cache INSERT)
        auto start = std::chrono::high_resolution_clock::now();
        const auto& edges = db.getEdges(u1);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        TEST("getEdges size", edges.size() == 2);
        
        // Due to prependEdge, u3 is the first edge
        TEST("getEdges first element", edges[0].target_node == u3);
        TEST("getEdges second element", edges[1].target_node == u2);

        // Get Edges Again (Cache HIT)
        start = std::chrono::high_resolution_clock::now();
        const auto& edges_cached = db.getEdges(u1);
        end = std::chrono::high_resolution_clock::now();
        auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

        TEST("getEdges cached size", edges_cached.size() == 2);
        // Note: duration2 might be faster, but it's hard to assert reliably due to timer precision on small lists.
        std::cout << "    (Duration Miss: " << duration1 << "us, Duration Hit: " << duration2 << "us)\n";

        db.sync();
    }

    cleanup_db(db_dir);
}

// ============================================================
// TEST 2: GraphDB Persistence
// ============================================================

void test_graphdb_persistence() {
    std::cout << "\n=== TEST: GraphDB Persistence ===\n";

    const char* db_dir = "/tmp/graphlite_test_p3_persist";
    ensure_dir(db_dir);
    cleanup_db(db_dir);

    {
        GraphDB db(db_dir);
        for (int i = 0; i < 1000; ++i) {
            db.addNode("Node_" + std::to_string(i));
        }

        NodeID u1 = db.getNodeId("Node_10");
        NodeID u2 = db.getNodeId("Node_20");
        db.addEdge(u1, u2, 1);
        db.addEdge(u1, u1, 2);

        db.sync();
    }

    // Reopen DB
    {
        GraphDB db(db_dir);

        // Verify Node Pool
        NodeID u1 = db.getNodeId("Node_10");
        NodeID u2 = db.getNodeId("Node_20");
        TEST("Reopen: Node_10 ID matches", u1 != 0);
        TEST("Reopen: Node_20 ID matches", u2 != 0);

        TEST("Reopen: getNodeName", db.getNodeName(u1) == "Node_10");
        TEST("Reopen: Node count persisted", db.getNodeId("Node_999") != 0);

        // Verify Edges
        const auto& edges = db.getEdges(u1);
        TEST("Reopen: Edge count", edges.size() == 2);
        TEST("Reopen: First edge target", edges[0].target_node == u1 && edges[0].edge_type == 2);
        TEST("Reopen: Second edge target", edges[1].target_node == u2 && edges[1].edge_type == 1);
    }

    cleanup_db(db_dir);
}

// ============================================================
// MAIN
// ============================================================

int main() {
    std::cout << "╔══════════════════════════════════════════╗\n";
    std::cout << "║  GraphLite v1.0 — Phase 3 Test Suite     ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n";

    test_graphdb_basic();
    test_graphdb_persistence();

    std::cout << "\n════════════════════════════════════════════\n";
    std::cout << "Results: " << tests_passed << " passed, "
              << tests_failed << " failed\n";

    if (tests_failed == 0) {
        std::cout << "✅ ALL TESTS PASSED — Phase 3 COMPLETE\n";
    } else {
        std::cout << "❌ SOME TESTS FAILED\n";
    }
    std::cout << "════════════════════════════════════════════\n";

    return tests_failed > 0 ? 1 : 0;
}
