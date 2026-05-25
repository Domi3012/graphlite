#include "../../include/graphlite/GraphDB.h"

using namespace graphlite;

GraphDB::GraphDB(const std::string& db_path) 
    : storage_(db_path), 
      schema_(), 
      node_dict_(50, 1000000), 
      adjacency_list_() {
}

NodeType GraphDB::defineNodeType(const std::string& type_name) {
    return schema_.registerNodeType(type_name);
}

EdgeType GraphDB::defineEdgeType(const std::string& type_name) {
    return schema_.registerEdgeType(type_name);
}

void GraphDB::ensureCapacityForNode(NodeID id) {
    while (adjacency_list_.size() <= id) {
        adjacency_list_.push_back(utils::MiniVector<GenericEdge>());
    }
}

NodeID GraphDB::addNode(const std::string& node_name) {
    NodeID id = node_dict_.get_or_create_id(node_name);
    ensureCapacityForNode(id);
    return id;
}

NodeID GraphDB::getNodeId(const std::string& node_name) {
    return node_dict_.get_id(node_name);
}

bool GraphDB::addEdge(NodeID from_id, NodeID to_id, EdgeType edge_type, 
                      const uint8_t* payload, uint8_t payload_size) {
    
    NodeID max_id = (from_id > to_id) ? from_id : to_id;
    ensureCapacityForNode(max_id);

    GenericEdge edge(to_id, edge_type, payload, payload_size);
    adjacency_list_[from_id].push_back(edge);

    return true;
}

const utils::MiniVector<GenericEdge>& GraphDB::getEdges(NodeID node_id) const {
    if (node_id >= adjacency_list_.size()) {
        static const utils::MiniVector<GenericEdge> empty_list;
        return empty_list;
    }
    return adjacency_list_[node_id];
}

void GraphDB::sync() {
    // Duyệt qua toàn bộ danh sách kề (Bỏ qua index 0 vì NodeID của StringPool bắt đầu từ 1)
    for (NodeID i = 1; i < adjacency_list_.size(); ++i) {
        const auto& edges = adjacency_list_[i];
        
        // Nếu đỉnh này không có Cạnh trỏ ra ngoài, không cần ghi xuống đĩa
        if (edges.size() == 0) {
            continue;
        }

        utils::MiniVector<uint8_t> byte_buffer;
        
        // Kỹ thuật Memory Reinterpreting (Ép kiểu vùng nhớ):
        // Nhìn toàn bộ mảng struct GenericEdge như một chuỗi byte thô
        const uint8_t* raw_bytes = reinterpret_cast<const uint8_t*>(edges.data());
        
        // Tổng số byte = Số lượng Cạnh * Kích thước 1 Cạnh (16 bytes)
        size_t total_bytes = edges.size() * sizeof(GenericEdge);
        
        // Đổ toàn bộ byte vào buffer
        for (size_t b = 0; b < total_bytes; ++b) {
            byte_buffer.push_back(raw_bytes[b]);
        }

        // Đẩy xuống Bitcask: Key là ID của Đỉnh, Value là mảng Cạnh của nó
        storage_.put(i, byte_buffer);
    }

    // Ra lệnh cho Bitcask xả (flush) toàn bộ OS Cache xuống mâm đĩa vật lý
    storage_.sync();
}

GraphDB::~GraphDB() {
    // Do nothing
}