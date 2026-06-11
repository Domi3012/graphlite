/**
 * @file test_phase1.cpp
 * @brief Phase 1 Verification — Test Platform, MmapFile, và các Utility classes.
 *
 * Test cases:
 * 1. MiniVector: constructors, iterators, resize
 * 2. NumericHashMap: put/get/remove, rehash
 * 3. StringPoolDictionary: get_or_create_id, get_string O(1)
 * 4. MmapFile: create, write, read, grow, sync
 * 5. BinaryUtils: raw pointer API
 */

#include <graphlite/graphlite.h>
#include <iostream>
#include <string>
#include <cstring>

using namespace graphlite;
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

// ============================================================
// TEST 1: MiniVector
// ============================================================

void test_minivector() {
    std::cout << "\n=== TEST: MiniVector ===\n";

    // Default constructor
    MiniVector<int> v1;
    TEST("Default constructor — size=0", v1.size() == 0);
    TEST("Default constructor — empty=true", v1.empty());

    // push_back
    v1.push_back(10);
    v1.push_back(20);
    v1.push_back(30);
    TEST("push_back — size=3", v1.size() == 3);
    TEST("push_back — v1[0]=10", v1[0] == 10);
    TEST("push_back — v1[2]=30", v1[2] == 30);

    // Sized constructor
    MiniVector<int> v2(100, 42);
    TEST("Sized constructor — size=100", v2.size() == 100);
    TEST("Sized constructor — v2[0]=42", v2[0] == 42);
    TEST("Sized constructor — v2[99]=42", v2[99] == 42);

    // Iterators (range-for)
    int sum = 0;
    for (auto& val : v1) {
        sum += val;
    }
    TEST("Range-for iterator — sum=60", sum == 60);

    // Resize up
    v1.resize(5, 99);
    TEST("Resize up — size=5", v1.size() == 5);
    TEST("Resize up — v1[3]=99", v1[3] == 99);
    TEST("Resize up — v1[4]=99", v1[4] == 99);
    TEST("Resize up — v1[0] preserved", v1[0] == 10);

    // Resize down
    v1.resize(2);
    TEST("Resize down — size=2", v1.size() == 2);
    TEST("Resize down — v1[0] preserved", v1[0] == 10);

    // Clear
    v1.clear();
    TEST("Clear — size=0", v1.size() == 0);
    TEST("Clear — empty=true", v1.empty());

    // Move semantics
    MiniVector<int> v3;
    v3.push_back(1);
    v3.push_back(2);
    MiniVector<int> v4(std::move(v3));
    TEST("Move constructor — v4.size=2", v4.size() == 2);
    TEST("Move constructor — v4[0]=1", v4[0] == 1);
    TEST("Move constructor — source emptied", v3.size() == 0);
}

// ============================================================
// TEST 2: NumericHashMap
// ============================================================

void test_numeric_hashmap() {
    std::cout << "\n=== TEST: NumericHashMap ===\n";

    NumericHashMap map(64);

    // Put and get
    map.put(100, 42);
    map.put(200, 84);
    size_t val = 0;
    TEST("put/get — key=100", map.get(100, val) && val == 42);
    TEST("put/get — key=200", map.get(200, val) && val == 84);
    TEST("get missing — key=300", !map.get(300, val));

    // Update
    map.put(100, 999);
    TEST("update — key=100 new value", map.get(100, val) && val == 999);

    // Remove
    TEST("remove — key=100", map.remove(100));
    TEST("remove — key=100 gone", !map.get(100, val));
    TEST("remove missing — key=999", !map.remove(999));

    // Stress: trigger rehash
    for (uint64_t i = 0; i < 1000; ++i) {
        map.put(i + 1000, i * 10);
    }
    bool all_found = true;
    for (uint64_t i = 0; i < 1000; ++i) {
        if (!map.get(i + 1000, val) || val != i * 10) {
            all_found = false;
            break;
        }
    }
    TEST("Stress 1000 entries + rehash", all_found);
    TEST("Size after stress", map.size() == 1001); // 1000 + key 200
}

// ============================================================
// TEST 3: StringPoolDictionary
// ============================================================

void test_string_pool() {
    std::cout << "\n=== TEST: StringPoolDictionary ===\n";

    StringPoolDictionary pool(1, 256); // 1MB pool, 256 slots

    // get_or_create_id
    uint32_t id_alice = pool.get_or_create_id("Alice");
    uint32_t id_bob   = pool.get_or_create_id("Bob");
    uint32_t id_carol = pool.get_or_create_id("Carol");
    TEST("IDs start at 1", id_alice == 1);
    TEST("IDs increment", id_bob == 2 && id_carol == 3);

    // Duplicate returns same ID
    uint32_t id_alice2 = pool.get_or_create_id("Alice");
    TEST("Duplicate — same ID", id_alice2 == id_alice);

    // get_id (read-only)
    TEST("get_id — exists", pool.get_id("Bob") == id_bob);
    TEST("get_id — not exists", pool.get_id("Dave") == 0);

    // get_string — O(1) reverse lookup
    TEST("get_string — Alice", pool.get_string(id_alice) == "Alice");
    TEST("get_string — Bob", pool.get_string(id_bob) == "Bob");
    TEST("get_string — Carol", pool.get_string(id_carol) == "Carol");
    TEST("get_string — invalid ID", pool.get_string(0) == "");
    TEST("get_string — out of range", pool.get_string(999) == "");

    // Stress: many strings
    for (int i = 0; i < 500; ++i) {
        pool.get_or_create_id("node_" + std::to_string(i));
    }
    TEST("Stress 500 strings — get back", 
         pool.get_string(pool.get_id("node_499")) == "node_499");
}

// ============================================================
// TEST 4: MmapFile
// ============================================================

void test_mmap_file() {
    std::cout << "\n=== TEST: MmapFile ===\n";

    const char* test_path = "/tmp/graphlite_test_phase1.dat";

    // Xóa file cũ nếu có
    std::remove(test_path);

    {
        // Tạo file mới
        internal::MmapFile file(test_path, 4096);
        TEST("Create — data not null", file.data() != nullptr);
        TEST("Create — capacity >= 4096", file.capacity() >= 4096);

        // Ghi dữ liệu
        auto* data = file.as<uint8_t>();
        const char* msg = "GraphLite v1.0 mmap test!";
        std::memcpy(data, msg, std::strlen(msg) + 1);

        // Đọc lại
        TEST("Write/Read — data matches", 
             std::strcmp(reinterpret_cast<char*>(data), msg) == 0);

        // Ghi struct tại offset
        struct TestHeader {
            uint32_t magic;
            uint32_t version;
        };
        auto* header = file.at<TestHeader>(256);
        header->magic = 0x474C4442;   // "GLDB"
        header->version = 100;

        auto* header_read = file.at<TestHeader>(256);
        TEST("Typed access at offset", 
             header_read->magic == 0x474C4442 && header_read->version == 100);

        // Grow
        size_t old_cap = file.capacity();
        file.grow(old_cap * 4);
        TEST("Grow — capacity increased", file.capacity() >= old_cap * 4);

        // Verify data survives grow (file-backed mmap giữ lại dữ liệu)
        auto* header_after_grow = file.at<TestHeader>(256);
        TEST("Data survives grow", 
             header_after_grow->magic == 0x474C4442 && header_after_grow->version == 100);

        // Sync
        file.sync();
        TEST("Sync — no crash", true);
    }

    // Mở lại file — verify persistence
    {
        internal::MmapFile file2(test_path);
        struct TestHeader {
            uint32_t magic;
            uint32_t version;
        };
        auto* header = file2.at<TestHeader>(256);
        TEST("Persistence — magic preserved", header->magic == 0x474C4442);
        TEST("Persistence — version preserved", header->version == 100);

        auto* data = file2.as<char>();
        TEST("Persistence — string preserved", 
             std::strcmp(data, "GraphLite v1.0 mmap test!") == 0);
    }

    // Cleanup
    std::remove(test_path);
}

// ============================================================
// TEST 5: BinaryUtils
// ============================================================

void test_binary_utils() {
    std::cout << "\n=== TEST: BinaryUtils ===\n";

    uint8_t buffer[64];
    std::memset(buffer, 0, 64);
    size_t offset = 0;

    // Raw pointer API
    writeUint16(buffer, offset, 0xABCD);
    writeUint32(buffer, offset, 0x12345678);
    writeUint64(buffer, offset, 0xDEADBEEFCAFEBABEULL);

    size_t read_offset = 0;
    TEST("readUint16", readUint16(buffer, read_offset) == 0xABCD);
    TEST("readUint32", readUint32(buffer, read_offset) == 0x12345678);
    TEST("readUint64", readUint64(buffer, read_offset) == 0xDEADBEEFCAFEBABEULL);
    TEST("Offset tracking", read_offset == 14); // 2 + 4 + 8
}

// ============================================================
// TEST 6: GenericEdge (types.h)
// ============================================================

void test_generic_edge() {
    std::cout << "\n=== TEST: GenericEdge ===\n";

    TEST("MAX_PAYLOAD_SIZE = 23", MAX_PAYLOAD_SIZE == 23);

    struct MyPayload {
        uint64_t timestamp;
        float    weight;
        uint32_t flags;
    };
    static_assert(sizeof(MyPayload) <= MAX_PAYLOAD_SIZE, "MyPayload fits in 23B");

    MyPayload p = {1234567890ULL, 3.14f, 0xFF};
    GenericEdge edge(42, 1, reinterpret_cast<uint8_t*>(&p), sizeof(p));

    TEST("Edge target", edge.target_node == 42);
    TEST("Edge type", edge.edge_type == 1);

    auto* recovered = reinterpret_cast<const MyPayload*>(edge.payload);
    TEST("Payload timestamp", recovered->timestamp == 1234567890ULL);
    TEST("Payload weight", recovered->weight == 3.14f);
    TEST("Payload flags", recovered->flags == 0xFF);

    // DiskEdge size
    TEST("DiskEdge is 32B", sizeof(DiskEdge) == 32);
    TEST("NodeRecord is 32B", sizeof(NodeRecord) == 32);
}

// ============================================================
// MAIN
// ============================================================

int main() {
    std::cout << "╔══════════════════════════════════════════╗\n";
    std::cout << "║  GraphLite v1.0 — Phase 1 Test Suite     ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n";

    test_minivector();
    test_numeric_hashmap();
    test_string_pool();
    test_mmap_file();
    test_binary_utils();
    test_generic_edge();

    std::cout << "\n════════════════════════════════════════════\n";
    std::cout << "Results: " << tests_passed << " passed, " 
              << tests_failed << " failed\n";
    
    if (tests_failed == 0) {
        std::cout << "✅ ALL TESTS PASSED — Phase 1 COMPLETE\n";
    } else {
        std::cout << "❌ SOME TESTS FAILED\n";
    }
    std::cout << "════════════════════════════════════════════\n";

    return tests_failed > 0 ? 1 : 0;
}
