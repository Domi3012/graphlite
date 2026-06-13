/**
 * @file test_phase2.cpp
 * @brief Phase 2 Verification — Test NodeStore, PageManager, và StringPool mmap mode.
 *
 * Test cases:
 * 1. NodeStore: alloc, getRecord O(1), grow, persistence
 * 2. PageManager: alloc/free pages/slots, free list reuse, getEdge O(1)
 * 3. readEdgeChain + prependEdge: linked list traversal
 * 4. StringPoolDictionary mmap mode: persistence across open/close
 * 5. End-to-end persistence: create → sync → close → reopen → verify
 */

#include <graphlite/graphlite.h>
#include "../src/storage/NodeStore.h"
#include "../src/storage/PageManager.h"
#include "../src/storage/MmapFile.h"
#include "../src/utils/StringPoolDictionary.h"
#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>

using namespace graphlite;
using namespace graphlite::internal;
using namespace graphlite::utils;

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

// ============================================================
// TEST 1: NodeStore
// ============================================================

void test_node_store() {
    std::cout << "\n=== TEST: NodeStore ===\n";

    const char* db_dir = "/tmp/graphlite_test_p2_nodes";
    ensure_dir(db_dir);
    remove_file("/tmp/graphlite_test_p2_nodes/nodes.gldb");

    {
        NodeStore store(db_dir);
        TEST("Initial nodeCount=0", store.nodeCount() == 0);
        TEST("Initial nextId=1", store.nextId() == 1);

        // Alloc nodes
        NodeID id1 = store.allocNodeId();
        NodeID id2 = store.allocNodeId();
        NodeID id3 = store.allocNodeId();
        TEST("First alloc id=1", id1 == 1);
        TEST("Second alloc id=2", id2 == 2);
        TEST("Third alloc id=3", id3 == 3);
        TEST("nodeCount=3", store.nodeCount() == 3);
        TEST("nextId=4", store.nextId() == 4);

        // getRecord O(1) — write + read
        NodeRecord* rec1 = store.getRecord(id1);
        rec1->flags = 1;
        rec1->node_type = 42;
        rec1->edge_count = 100;

        const NodeRecord* rec1_read = store.getRecord(id1);
        TEST("getRecord flags=1", rec1_read->flags == 1);
        TEST("getRecord node_type=42", rec1_read->node_type == 42);
        TEST("getRecord edge_count=100", rec1_read->edge_count == 100);
        TEST("getRecord first_edge_page=NULL_PAGE", rec1_read->first_edge_page == NULL_PAGE);

        // Alloc many nodes (test grow)
        for (int i = 0; i < 10000; ++i) {
            store.allocNodeId();
        }
        TEST("After 10K allocs — nodeCount=10003", store.nodeCount() == 10003);
        TEST("After 10K allocs — rec1 preserved", store.getRecord(id1)->node_type == 42);

        store.sync();
    }

    // Persistence test — reopen
    {
        NodeStore store2(db_dir);
        TEST("Reopen — nodeCount=10003", store2.nodeCount() == 10003);
        TEST("Reopen — nextId=10004", store2.nextId() == 10004);
        TEST("Reopen — rec1 node_type=42", store2.getRecord(1)->node_type == 42);
        TEST("Reopen — rec1 flags=1", store2.getRecord(1)->flags == 1);
        TEST("Reopen — rec1 edge_count=100", store2.getRecord(1)->edge_count == 100);
    }

    remove_file("/tmp/graphlite_test_p2_nodes/nodes.gldb");
}

// ============================================================
// TEST 2: PageManager — page/slot allocation & free lists
// ============================================================

void test_page_manager() {
    std::cout << "\n=== TEST: PageManager ===\n";

    const char* db_dir = "/tmp/graphlite_test_p2_edges";
    ensure_dir(db_dir);
    remove_file("/tmp/graphlite_test_p2_edges/edges.gldb");

    {
        PageManager mgr(db_dir);
        TEST("Initial pageCount=16", mgr.pageCount() == 16);

        // Alloc page
        uint16_t page0 = mgr.allocPage();
        TEST("allocPage returns valid page", page0 != NULL_SLOT);

        // Alloc slots within the page
        uint16_t slot0 = mgr.allocSlot(page0);
        uint16_t slot1 = mgr.allocSlot(page0);
        uint16_t slot2 = mgr.allocSlot(page0);
        TEST("allocSlot — slot0 valid", slot0 != NULL_SLOT);
        TEST("allocSlot — slot1 valid", slot1 != NULL_SLOT);
        TEST("allocSlot — slots are different", slot0 != slot1 && slot1 != slot2);

        // Write to edge
        DiskEdge* edge0 = mgr.getEdge(page0, slot0);
        edge0->source_node = 1;
        edge0->target_node = 42;
        edge0->edge_type = 7;
        edge0->next_page = NULL_PAGE;
        edge0->next_slot = NULL_SLOT;
        edge0->next_in_page = NULL_PAGE;
        edge0->next_in_slot = NULL_SLOT;

        // Read back
        const DiskEdge* edge0_read = mgr.getEdge(page0, slot0);
        TEST("getEdge — target=42", edge0_read->target_node == 42);
        TEST("getEdge — type=7", edge0_read->edge_type == 7);

        // Free slot and reuse
        mgr.freeSlot(page0, slot1);
        uint16_t slot_reused = mgr.allocSlot(page0);
        TEST("freeSlot+alloc — slot reused", slot_reused == slot1);

        // hasAvailableSlot
        TEST("hasAvailableSlot — yes", mgr.hasAvailableSlot(page0));

        // Free page and reuse
        uint16_t page1 = mgr.allocPage();
        mgr.freePage(page1);
        uint16_t page_reused = mgr.allocPage();
        TEST("freePage+alloc — page reused", page_reused == page1);

        // Fill slots until page is full
        uint16_t fill_page = mgr.allocPage();
        int alloc_count = 0;
        while (mgr.allocSlot(fill_page) != NULL_SLOT) {
            alloc_count++;
        }
        TEST("Page full at 126 slots", alloc_count == EDGES_PER_PAGE);
        TEST("hasAvailableSlot — no (full)", !mgr.hasAvailableSlot(fill_page));

        mgr.sync();
    }

    // Persistence
    {
        PageManager mgr2(db_dir);
        TEST("Reopen — pageCount preserved", mgr2.pageCount() >= 16);
    }

    remove_file("/tmp/graphlite_test_p2_edges/edges.gldb");
}

// ============================================================
// TEST 3: readEdgeChain + prependEdge
// ============================================================

void test_edge_chain() {
    std::cout << "\n=== TEST: Edge Chain (readEdgeChain + prependEdge) ===\n";

    const char* db_dir = "/tmp/graphlite_test_p2_chain";
    ensure_dir(db_dir);
    remove_file("/tmp/graphlite_test_p2_chain/edges.gldb");

    {
        PageManager mgr(db_dir);

        uint32_t out_chain_page = NULL_PAGE;
        uint16_t out_chain_slot = NULL_SLOT;
        uint32_t in_chain_page = NULL_PAGE;
        uint16_t in_chain_slot = NULL_SLOT;

        // Prepend 100 edges
        for (int i = 0; i < 100; ++i) {
            GenericEdge edge;
            edge.source_node = 999;
            edge.target_node = static_cast<NodeID>(i + 1);
            edge.edge_type = 1;
            std::memset(edge.payload, 0, MAX_PAYLOAD_SIZE);
            // Store index in payload
            uint32_t idx = static_cast<uint32_t>(i);
            std::memcpy(edge.payload, &idx, sizeof(uint32_t));

            mgr.prependBidirectionalEdge(out_chain_page, out_chain_slot, in_chain_page, in_chain_slot, edge);
        }

        TEST("Chain head not NULL after prepend", out_chain_page != NULL_PAGE);

        // Read back
        MiniVector<GenericEdge> edges;
        mgr.readEdgeChain(out_chain_page, out_chain_slot, true, edges);
        TEST("readEdgeChain — 100 edges", edges.size() == 100);

        // Prepend pushes newest first → reversed order
        // Edge 99 was prepended last → should be first
        TEST("readEdgeChain — first edge target=100",
             edges[0].target_node == 100);
        TEST("readEdgeChain — last edge target=1",
             edges[99].target_node == 1);

        // Verify payload
        uint32_t first_idx;
        std::memcpy(&first_idx, edges[0].payload, sizeof(uint32_t));
        TEST("readEdgeChain — first payload=99", first_idx == 99);

        mgr.sync();
    }

    // Persistence — reopen and read chain
    // (Note: We'd need to persist chain_page/chain_slot via NodeStore in real use.
    //  For this test, we verify file didn't corrupt — data is on mmap.)

    remove_file("/tmp/graphlite_test_p2_chain/edges.gldb");
}

// ============================================================
// TEST 4: StringPoolDictionary mmap mode
// ============================================================

void test_string_pool_mmap() {
    std::cout << "\n=== TEST: StringPoolDictionary (mmap mode) ===\n";

    const char* file_path = "/tmp/graphlite_test_p2_strings.gldb";
    remove_file(file_path);

    {
        StringPoolDictionary pool{std::string(file_path)};
        TEST("isMmapMode=true", pool.isMmapMode());

        uint32_t id_alice = pool.get_or_create_id("Alice");
        uint32_t id_bob   = pool.get_or_create_id("Bob");
        uint32_t id_carol = pool.get_or_create_id("Carol");
        TEST("mmap — IDs start at 1", id_alice == 1);
        TEST("mmap — IDs increment", id_bob == 2 && id_carol == 3);

        // Duplicate
        uint32_t id_alice2 = pool.get_or_create_id("Alice");
        TEST("mmap — duplicate same ID", id_alice2 == id_alice);

        // get_string
        TEST("mmap — get_string Alice", pool.get_string(id_alice) == "Alice");
        TEST("mmap — get_string Bob", pool.get_string(id_bob) == "Bob");

        // Stress: many strings
        for (int i = 0; i < 500; ++i) {
            pool.get_or_create_id("mmap_node_" + std::to_string(i));
        }
        TEST("mmap — stress 500 strings get back",
             pool.get_string(pool.get_id("mmap_node_499")) == "mmap_node_499");

        pool.sync();
    }

    // Persistence — reopen
    {
        StringPoolDictionary pool2{std::string(file_path)};
        TEST("mmap reopen — Alice exists", pool2.get_id("Alice") == 1);
        TEST("mmap reopen — Bob exists", pool2.get_id("Bob") == 2);
        TEST("mmap reopen — Carol exists", pool2.get_id("Carol") == 3);
        TEST("mmap reopen — get_string Alice", pool2.get_string(1) == "Alice");
        TEST("mmap reopen — stress data persisted",
             pool2.get_string(pool2.get_id("mmap_node_499")) == "mmap_node_499");

        // Can still add new entries after reopen
        uint32_t new_id = pool2.get_or_create_id("Dave");
        TEST("mmap reopen — new entry after reopen", new_id > 3);
        TEST("mmap reopen — get_string Dave", pool2.get_string(new_id) == "Dave");
    }

    remove_file(file_path);
}

// ============================================================
// TEST 5: End-to-end Persistence (NodeStore + PageManager together)
// ============================================================

void test_e2e_persistence() {
    std::cout << "\n=== TEST: End-to-End Persistence ===\n";

    const char* db_dir = "/tmp/graphlite_test_p2_e2e";
    ensure_dir(db_dir);
    remove_file("/tmp/graphlite_test_p2_e2e/nodes.gldb");
    remove_file("/tmp/graphlite_test_p2_e2e/edges.gldb");

    uint32_t saved_chain_page;
    uint16_t saved_chain_slot;

    {
        NodeStore nodes(db_dir);
        PageManager edges(db_dir);

        // Create node
        NodeID nid = nodes.allocNodeId();
        NodeRecord* rec = nodes.getRecord(nid);
        rec->flags = 1;
        rec->node_type = 5;

        // Prepend 10 edges
        uint32_t out_chain_page = NULL_PAGE;
        uint16_t out_chain_slot = NULL_SLOT;
        uint32_t in_chain_page = NULL_PAGE;
        uint16_t in_chain_slot = NULL_SLOT;

        for (int i = 0; i < 10; ++i) {
            GenericEdge e;
            e.source_node = nid;
            e.target_node = static_cast<NodeID>(100 + i);
            e.edge_type = 2;
            std::memset(e.payload, 0, MAX_PAYLOAD_SIZE);
            edges.prependBidirectionalEdge(out_chain_page, out_chain_slot, in_chain_page, in_chain_slot, e);
        }

        // Link chain head to node
        rec = nodes.getRecord(nid);  // Re-fetch after potential remap
        rec->first_edge_page = out_chain_page;
        rec->first_edge_slot = out_chain_slot;
        rec->edge_count = 10;
        
        // Also update in-edge head (even though nid is not the target, we just simulate)
        rec->first_in_edge_page = in_chain_page;
        rec->first_in_edge_slot = in_chain_slot;
        rec->in_edge_count = 10;

        saved_chain_page = out_chain_page;
        saved_chain_slot = out_chain_slot;

        nodes.sync();
        edges.sync();
    }

    // Reopen and verify everything
    {
        NodeStore nodes2(db_dir);
        PageManager edges2(db_dir);

        TEST("E2E — nodeCount=1", nodes2.nodeCount() == 1);

        const NodeRecord* rec = nodes2.getRecord(1);
        TEST("E2E — node flags=1", rec->flags == 1);
        TEST("E2E — node type=5", rec->node_type == 5);
        TEST("E2E — node edge_count=10", rec->edge_count == 10);
        TEST("E2E — chain_page preserved", rec->first_edge_page == saved_chain_page);
        TEST("E2E — chain_slot preserved", rec->first_edge_slot == saved_chain_slot);

        // Read edge chain
        MiniVector<GenericEdge> chain;
        edges2.readEdgeChain(rec->first_edge_page, rec->first_edge_slot, true, chain);
        TEST("E2E — chain has 10 edges", chain.size() == 10);
        // Last prepended = 109, should be first in chain
        TEST("E2E — first edge target=109", chain[0].target_node == 109);
        TEST("E2E — last edge target=100", chain[9].target_node == 100);
    }

    remove_file("/tmp/graphlite_test_p2_e2e/nodes.gldb");
    remove_file("/tmp/graphlite_test_p2_e2e/edges.gldb");
}

// ============================================================
// MAIN
// ============================================================

int main() {
    std::cout << "╔══════════════════════════════════════════╗\n";
    std::cout << "║  GraphLite v1.0 — Phase 2 Test Suite     ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n";

    test_node_store();
    test_page_manager();
    test_edge_chain();
    test_string_pool_mmap();
    test_e2e_persistence();

    std::cout << "\n════════════════════════════════════════════\n";
    std::cout << "Results: " << tests_passed << " passed, "
              << tests_failed << " failed\n";

    if (tests_failed == 0) {
        std::cout << "✅ ALL TESTS PASSED — Phase 2 COMPLETE\n";
    } else {
        std::cout << "❌ SOME TESTS FAILED\n";
    }
    std::cout << "════════════════════════════════════════════\n";

    return tests_failed > 0 ? 1 : 0;
}
