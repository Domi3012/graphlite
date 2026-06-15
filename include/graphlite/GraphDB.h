/**
 * @file GraphDB.h
 * @brief Giao diện cốt lõi của GraphLite Database.
 * @version 1.0 (STUB — sẽ implement đầy đủ ở Phase 3)
 */

#pragma once
#include <string>
#include <cstdint>
#include <memory>

#include "types.h"
#include "MiniVector.h"

namespace graphlite {

/**
 * @class GraphDB
 * @brief Trái tim của hệ thống GraphLite — quản lý vòng đời đồ thị.
 */
class GraphDB {
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;

public:
    /**
     * @brief Khởi tạo Database.
     * @param db_dir Đường dẫn đến thư mục chứa database files.
     */
    explicit GraphDB(const std::string& db_dir);
    ~GraphDB();

    // Chặn copy
    GraphDB(const GraphDB&) = delete;
    GraphDB& operator=(const GraphDB&) = delete;
    GraphDB(GraphDB&&) noexcept;
    GraphDB& operator=(GraphDB&&) noexcept;

    // --- Schema API ---
    NodeType defineNodeType(const std::string& type_name);
    EdgeType defineEdgeType(const std::string& type_name);

    // --- Graph API ---
    NodeID addNode(const std::string& node_name);
    NodeID getNodeId(const std::string& node_name);
    std::string getNodeName(NodeID id) const;
    size_t getNodeCount() const;
    bool addEdge(NodeID from_id, NodeID to_id, EdgeType edge_type,
                 const uint8_t* payload = nullptr, uint8_t payload_size = 0);
    const utils::MiniVector<GenericEdge>& getOutEdges(NodeID node_id) const;
    const utils::MiniVector<GenericEdge>& getInEdges(NodeID node_id) const;

    // --- Storage API ---
    void sync();
};

} // namespace graphlite