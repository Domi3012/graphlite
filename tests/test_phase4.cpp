/**
 * @file test_phase4.cpp
 * @brief Kiểm thử Phase 4: Traversal Engine (DFS Iterative, BFS, MiniQueue, utilities).
 */

#include <graphlite/graphlite.h>
#include <graphlite/traversal.h>
#include <graphlite/internal/MiniQueue.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>

using namespace graphlite;
using namespace graphlite::utils;

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

// Callback dùng để lưu danh sách đỉnh đã duyệt
class CollectCallback : public ITraversalCallback {
public:
    MiniVector<NodeID> visited_nodes;

    bool shouldTraverse(NodeID current_node, const GenericEdge& edge) override {
        return true; // Traverse everything
    }

    void onNodeVisited(NodeID node) override {
        visited_nodes.push_back(node);
    }
};

void test_miniqueue() {
    std::cout << "\n=== TEST: MiniQueue ===\n";
    MiniQueue<int> q(2);
    
    TEST("Init empty", q.empty() && q.size() == 0);
    
    q.push(10);
    q.push(20);
    TEST("Push 2 elements", q.size() == 2 && q.front() == 10);
    
    q.push(30); // Resize triggers
    TEST("Push triggers resize", q.size() == 3 && q.front() == 10);
    
    q.pop();
    TEST("Pop front", q.size() == 2 && q.front() == 20);
    
    q.push(40);
    q.push(50);
    TEST("Push after pop", q.size() == 4 && q.front() == 20);
    
    q.pop(); q.pop(); q.pop(); q.pop();
    TEST("Empty after pop all", q.empty());
}

void test_traversal() {
    std::cout << "\n=== TEST: Traversal Engine (DFS/BFS) ===\n";
    
    const char* db_dir = "/tmp/graphlite_test_p4";
    ensure_dir(db_dir);
    cleanup_db(db_dir);

    {
        GraphDB db(db_dir);
        
        EdgeType follow = db.defineEdgeType("FOLLOW");
        
        // Tạo đồ thị:
        // 1 -> 2 -> 4
        // 1 -> 3 -> 5
        NodeID n1 = db.addNode("N1");
        NodeID n2 = db.addNode("N2");
        NodeID n3 = db.addNode("N3");
        NodeID n4 = db.addNode("N4");
        NodeID n5 = db.addNode("N5");
        
        db.addEdge(n1, n2, follow);
        db.addEdge(n1, n3, follow);
        db.addEdge(n2, n4, follow);
        db.addEdge(n3, n5, follow);

        TraversalEngine engine(db);

        // Test DFS
        CollectCallback dfs_cb;
        engine.dfs(n1, 10, dfs_cb);
        TEST("DFS size", dfs_cb.visited_nodes.size() == 5);
        // DFS thứ tự (giả sử add n2, n3 thì out-edges có n3, n2 do prepend.
        // Stack push n2, n3. Tùy thuộc loop ngược, có thể theo thứ tự n2, n4, n3, n5
        TEST("DFS contains start", dfs_cb.visited_nodes[0] == n1);

        // Test BFS
        CollectCallback bfs_cb;
        engine.bfs(n1, 10, bfs_cb);
        TEST("BFS size", bfs_cb.visited_nodes.size() == 5);
        TEST("BFS level 0", bfs_cb.visited_nodes[0] == n1);
        // Level 1: N2, N3 (hoặc ngược lại)
        bool level1_ok = (bfs_cb.visited_nodes[1] == n2 || bfs_cb.visited_nodes[1] == n3) &&
                         (bfs_cb.visited_nodes[2] == n2 || bfs_cb.visited_nodes[2] == n3);
        TEST("BFS level 1", level1_ok);
        
        // Test Utilities
        MiniVector<EdgeType> pattern;
        pattern.push_back(follow);
        pattern.push_back(follow);
        
        auto matched = MatchPathPattern(db, n1, pattern);
        TEST("MatchPathPattern length 2", matched.size() == 2); // N4 and N5
        
        size_t count_n1_out = CountNeighborsIf(db, n1, follow, true);
        size_t count_n4_in = CountNeighborsIf(db, n4, follow, false);
        TEST("CountNeighborsIf OUT", count_n1_out == 2);
        TEST("CountNeighborsIf IN", count_n4_in == 1);
    }
    
    cleanup_db(db_dir);
}

int main() {
    std::cout << "╔══════════════════════════════════════════╗\n";
    std::cout << "║  GraphLite v0.2 — Phase 4 Test Suite     ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n";

    test_miniqueue();
    test_traversal();

    std::cout << "\n════════════════════════════════════════════\n";
    std::cout << "Results: " << tests_passed << " passed, "
              << tests_failed << " failed\n";

    if (tests_failed == 0) {
        std::cout << "✅ ALL TESTS PASSED — Phase 4 COMPLETE\n";
    } else {
        std::cout << "❌ SOME TESTS FAILED\n";
    }
    std::cout << "════════════════════════════════════════════\n";

    return tests_failed > 0 ? 1 : 0;
}
