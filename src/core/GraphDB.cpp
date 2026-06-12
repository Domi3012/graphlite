#include <graphlite/GraphDB.h>
#include <graphlite/Schema.h>
#include "../utils/StringPoolDictionary.h"
#include <stdexcept>

namespace graphlite {

struct GraphDB::Impl {
    Schema schema_;
    utils::StringPoolDictionary node_dict_;
    utils::MiniVector<utils::MiniVector<GenericEdge>> adjacency_list_;

    Impl() : schema_(), node_dict_(4, 65536), adjacency_list_() {}

    void ensureCapacityForNode(NodeID id) {
        while (adjacency_list_.size() <= id) {
            adjacency_list_.push_back(utils::MiniVector<GenericEdge>());
        }
    }
};

GraphDB::GraphDB(const std::string& db_dir) : pimpl_(std::make_unique<Impl>()) {
    // TODO Phase 3: Mở mmap files trong db_dir
    (void)db_dir;
}

GraphDB::~GraphDB() = default;

GraphDB::GraphDB(GraphDB&&) noexcept = default;
GraphDB& GraphDB::operator=(GraphDB&&) noexcept = default;

NodeType GraphDB::defineNodeType(const std::string& type_name) {
    return pimpl_->schema_.registerNodeType(type_name);
}

EdgeType GraphDB::defineEdgeType(const std::string& type_name) {
    return pimpl_->schema_.registerEdgeType(type_name);
}

NodeID GraphDB::addNode(const std::string& node_name) {
    NodeID id = pimpl_->node_dict_.get_or_create_id(node_name);
    pimpl_->ensureCapacityForNode(id);
    return id;
}

NodeID GraphDB::getNodeId(const std::string& node_name) {
    return pimpl_->node_dict_.get_id(node_name);
}

std::string GraphDB::getNodeName(NodeID id) const {
    return pimpl_->node_dict_.get_string(id);
}

bool GraphDB::addEdge(NodeID from_id, NodeID to_id, EdgeType edge_type,
                      const uint8_t* payload, uint8_t payload_size) {
    NodeID max_id = (from_id > to_id) ? from_id : to_id;
    pimpl_->ensureCapacityForNode(max_id);
    GenericEdge edge(to_id, edge_type, payload, payload_size);
    pimpl_->adjacency_list_[from_id].push_back(edge);
    return true;
}

const utils::MiniVector<GenericEdge>& GraphDB::getEdges(NodeID node_id) const {
    if (node_id >= pimpl_->adjacency_list_.size()) {
        static const utils::MiniVector<GenericEdge> empty_list;
        return empty_list;
    }
    return pimpl_->adjacency_list_[node_id];
}

void GraphDB::sync() {
    // TODO Phase 3: msync() tất cả mmap files
}

} // namespace graphlite
