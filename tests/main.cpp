#include <iostream>
#include <string>
#include <graphlite/GraphDB.h>
#include <graphlite/traversal.h>

using namespace graphlite;

// 1. CẤU TRÚC PAYLOAD THỬ NGHIỆM TỔNG QUÁT (8 bytes)
struct TestPayload {
    uint32_t timestamp;
    uint32_t weight;
};

// 2. CHIẾN LƯỢC DUYỆT THỬ NGHIỆM (Chỉ đi qua cạnh có weight > 50)
class WeightFilterStrategy : public ITraversalCallback {
public:
    bool shouldTraverse(NodeID current, const GenericEdge& edge) override {
        const TestPayload* p = reinterpret_cast<const TestPayload*>(edge.payload);
        
        // Cắt tỉa nhánh: Trọng số nhỏ hơn bằng 50 thì không đi
        if (p->weight <= 50) {
            return false; 
        }
        return true; 
    }

    void onNodeVisited(NodeID node) override {
        std::cout << " -> Đã đi qua Node_ID: " << node << "\n";
    }
};

int main() {
    std::cout << "=== GRAPHLITE ENGINE: TRAVERSAL TEST ===\n";

    // Khởi tạo Database nội bộ
    GraphDB db("test_engine.db");

    // Đăng ký Schema
    EdgeType CONNECT = db.defineEdgeType("CONNECT");

    std::cout << "[1] Đang tạo mạng lưới đồ thị thử nghiệm...\n";
    NodeID n1 = db.addNode("Node_A");
    NodeID n2 = db.addNode("Node_B");
    NodeID n3 = db.addNode("Node_C");
    NodeID n4 = db.addNode("Node_D");

    // Tạo Payload
    TestPayload p_strong1 = {1000, 80}; // Weight 80
    TestPayload p_strong2 = {1005, 95}; // Weight 95
    TestPayload p_weak    = {1010, 10}; // Weight 10 (Sẽ bị chặn)

    // Nối A -> B -> C (Đường đi mạnh)
    db.addEdge(n1, n2, CONNECT, reinterpret_cast<uint8_t*>(&p_strong1), sizeof(p_strong1));
    db.addEdge(n2, n3, CONNECT, reinterpret_cast<uint8_t*>(&p_strong2), sizeof(p_strong2));
    
    // Nối B -> D (Đường đi yếu)
    db.addEdge(n2, n4, CONNECT, reinterpret_cast<uint8_t*>(&p_weak), sizeof(p_weak));

    std::cout << "[2] Chạy DFS từ Node_A (Lọc các cạnh có weight <= 50)...\n";
    
    TraversalEngine engine(db);
    WeightFilterStrategy filter; 
    
    std::cout << "Hành trình mong đợi: Node_A -> Node_B -> Node_C (Bỏ qua Node_D)\n";
    std::cout << "Hành trình thực tế:\n";
    engine.dfs(n1, 5, filter);

    std::cout << "[3] Gọi sync() để chốt I/O...\n";
    db.sync();

    std::cout << "=== TEST HOÀN TẤT ===\n";
    return 0;
}