/**
 * @file GraphDB.h
 * @brief Giao diện cốt lõi của GraphLite Database.
 * @version 1.0 (STUB — sẽ implement đầy đủ ở Phase 3)
 *
 * GraphLite v1.0 sử dụng kiến trúc hybrid:
 * - Dữ liệu lưu trên mmap files (nodes.gldb, edges.gldb, strings.gldb)
 * - Hot nodes cache trong RAM bằng CLOCK algorithm
 * - Index-free adjacency: edges liên kết thành linked list trên đĩa
 */

#pragma once
#include <string>
#include <cstdint>

#include "types.h"
#include "Schema.h"
#include "internal/MiniVector.h"
#include "internal/StringPoolDictionary.h"

namespace graphlite {

/**
 * @class GraphDB
 * @brief Trái tim của hệ thống GraphLite — quản lý vòng đời đồ thị.
 *
 * @warning STUB — Phase 3 sẽ implement đầy đủ với mmap backend.
 *          Hiện tại chỉ chứa declarations để project compile được.
 */
class GraphDB {
private:
    Schema schema_;
    utils::StringPoolDictionary node_dict_;
    utils::MiniVector<utils::MiniVector<GenericEdge>> adjacency_list_;

    void ensureCapacityForNode(NodeID id);

public:
    /**
     * @brief Khởi tạo Database.
     * @param db_dir Đường dẫn đến thư mục chứa database files.
     *
     * @note v1.0: Nhận thư mục (tạo nodes.gldb, edges.gldb, strings.gldb bên trong).
     *       v0.1: Nhận file path đơn lẻ.
     */
    explicit GraphDB(const std::string& db_dir);
    ~GraphDB();

    // Chặn copy
    GraphDB(const GraphDB&) = delete;
    GraphDB& operator=(const GraphDB&) = delete;

    // --- Schema API ---
    NodeType defineNodeType(const std::string& type_name);
    EdgeType defineEdgeType(const std::string& type_name);

    // --- Graph API ---
    NodeID addNode(const std::string& node_name);
    NodeID getNodeId(const std::string& node_name);
    std::string getNodeName(NodeID id) const;
    bool addEdge(NodeID from_id, NodeID to_id, EdgeType edge_type,
                 const uint8_t* payload = nullptr, uint8_t payload_size = 0);
    const utils::MiniVector<GenericEdge>& getEdges(NodeID node_id) const;

    // --- Storage API ---
    void sync();
};

// ============================================================
// STUB IMPLEMENTATION (tạm thời — sẽ được thay thế ở Phase 3)
// ============================================================

#ifdef GRAPHLITE_IMPL_GUARD

GRAPHLITE_FUNC GraphDB::GraphDB(const std::string& db_dir)
    : schema_(), node_dict_(4, 65536), adjacency_list_() {
    // TODO Phase 3: Mở mmap files trong db_dir
    (void)db_dir;
}

GRAPHLITE_FUNC GraphDB::~GraphDB() {}

GRAPHLITE_FUNC NodeType GraphDB::defineNodeType(const std::string& type_name) {
    return schema_.registerNodeType(type_name);
}

GRAPHLITE_FUNC EdgeType GraphDB::defineEdgeType(const std::string& type_name) {
    return schema_.registerEdgeType(type_name);
}

GRAPHLITE_FUNC void GraphDB::ensureCapacityForNode(NodeID id) {
    while (adjacency_list_.size() <= id) {
        adjacency_list_.push_back(utils::MiniVector<GenericEdge>());
    }
}

GRAPHLITE_FUNC NodeID GraphDB::addNode(const std::string& node_name) {
    NodeID id = node_dict_.get_or_create_id(node_name);
    ensureCapacityForNode(id);
    return id;
}

GRAPHLITE_FUNC NodeID GraphDB::getNodeId(const std::string& node_name) {
    return node_dict_.get_id(node_name);
}

GRAPHLITE_FUNC std::string GraphDB::getNodeName(NodeID id) const {
    return node_dict_.get_string(id);
}

GRAPHLITE_FUNC bool GraphDB::addEdge(NodeID from_id, NodeID to_id, EdgeType edge_type,
                                      const uint8_t* payload, uint8_t payload_size) {
    NodeID max_id = (from_id > to_id) ? from_id : to_id;
    ensureCapacityForNode(max_id);
    GenericEdge edge(to_id, edge_type, payload, payload_size);
    adjacency_list_[from_id].push_back(edge);
    return true;
}

GRAPHLITE_FUNC const utils::MiniVector<GenericEdge>& GraphDB::getEdges(NodeID node_id) const {
    if (node_id >= adjacency_list_.size()) {
        static const utils::MiniVector<GenericEdge> empty_list;
        return empty_list;
    }
    return adjacency_list_[node_id];
}

GRAPHLITE_FUNC void GraphDB::sync() {
    // TODO Phase 3: msync() tất cả mmap files
}

#endif // GRAPHLITE_IMPL_GUARD

} // namespace graphlite