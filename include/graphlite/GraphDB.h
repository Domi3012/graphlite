#pragma once
#include <string>
#include <cstdint>

// Nhúng các mảnh ghép chúng ta đã xây dựng
#include "types.h"
#include "Schema.h"
#include "../../src/utils/MiniVector.h"
#include "../../src/utils/StringPoolDictionary.h"
#include "../../src/engine/BitcaskEngine.h"

namespace graphlite {

class GraphDB {
private:
    // 1. Tầng I/O Vật lý
    BitcaskEngine storage_;

    // 2. Tầng Siêu dữ liệu (Dịch "User", "USES" sang số nguyên)
    Schema schema_;

    // 3. Sổ hộ khẩu toàn cục: Dịch Tên Đỉnh ("U001") thành ID (1, 2, 3...)
    utils::StringPoolDictionary node_dict_;

    // 4. TRÁI TIM CỦA HỆ THỐNG: Danh sách kề trên RAM
    // Tại sao lại là mảng của mảng? 
    // Vì NodeID là số nguyên liên tiếp. Tra cứu adjacency_list_[NodeID] là O(1) tuyệt đối!
    utils::MiniVector< utils::MiniVector<GenericEdge> > adjacency_list_;

    // Hàm nội bộ: Đảm bảo danh sách kề đủ sức chứa cho một NodeID mới
    void ensureCapacityForNode(NodeID id);

public:
    // Constructor: Chỉ cần truyền đường dẫn file database
    explicit GraphDB(const std::string& db_path);
    ~GraphDB();

    // Chặn Copy
    GraphDB(const GraphDB&) = delete;
    GraphDB& operator=(const GraphDB&) = delete;

    // ==========================================
    // API TẦNG SCHEMA (Cấu hình đồ thị)
    // ==========================================
    NodeType defineNodeType(const std::string& type_name);
    EdgeType defineEdgeType(const std::string& type_name);

    // ==========================================
    // API TẦNG GRAPH (Tương tác mạng lưới)
    // ==========================================
    
    // Thêm Đỉnh: Trả về ID nguyên thủy của Đỉnh
    NodeID addNode(const std::string& node_name);

    // Lấy ID của một Đỉnh (Nếu không tồn tại trả về 0)
    NodeID getNodeId(const std::string& node_name);

    // Thêm Cạnh mang Payload mờ (Opaque Payload)
    bool addEdge(NodeID from_id, NodeID to_id, EdgeType edge_type, 
                 const uint8_t* payload = nullptr, uint8_t payload_size = 0);

    // Lấy danh sách hàng xóm của một Đỉnh
    // Trả về tham chiếu const để cấm Application sửa trực tiếp dữ liệu trên RAM của DB
    const utils::MiniVector<GenericEdge>& getEdges(NodeID node_id) const;

    // ==========================================
    // API TẦNG STORAGE (Đồng bộ I/O)
    // ==========================================
    
    // Đóng gói Graph trên RAM và nã xuống Bitcask
    void sync();
};

} // namespace graphlite