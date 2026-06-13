/**
 * @file GraphDB.cpp
 * @brief Implementation đầy đủ của GraphDB — tích hợp mmap storage engine.
 * @version 2.0 (Phase 3)
 *
 * GraphDB::Impl chứa:
 * - NodeStore: flat array NodeRecord trên mmap (nodes.gldb)
 * - PageManager: page/slot allocation cho edges trên mmap (edges.gldb)
 * - StringPoolDictionary: mmap-backed string ↔ ID (strings.gldb)
 * - ClockCache: edge list cache cho hot nodes
 * - Schema: metadata types (heap-backed, nhẹ)
 */

#include <graphlite/GraphDB.h>
#include <graphlite/Schema.h>
#include "../storage/NodeStore.h"
#include "../storage/PageManager.h"
#include "../utils/StringPoolDictionary.h"
#include "../utils/ClockCache.h"
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>

namespace graphlite {

// ============================================================
// IMPL — Private implementation (PIMPL)
// ============================================================

struct GraphDB::Impl {
    std::string db_dir_;
    Schema schema_;
    internal::NodeStore node_store_;
    internal::PageManager page_manager_;
    utils::StringPoolDictionary string_pool_;
    internal::ClockCache<utils::MiniVector<GenericEdge>> edge_cache_;

    // Cached empty list trả về khi node không có edges
    static const utils::MiniVector<GenericEdge>& emptyEdgeList() {
        static const utils::MiniVector<GenericEdge> empty;
        return empty;
    }

    /**
     * @brief Constructor — mở tất cả storage files.
     * @param db_dir Thư mục database (sẽ tạo nếu chưa tồn tại).
     */
    explicit Impl(const std::string& db_dir)
        : db_dir_(db_dir),
          schema_(),
          node_store_(db_dir),
          page_manager_(db_dir),
          string_pool_(db_dir + "/strings.gldb"),
          edge_cache_(8192) {}

    // Không cho copy
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
};

// ============================================================
// HELPER: tạo thư mục nếu chưa tồn tại
// ============================================================

static void ensure_directory(const std::string& path) {
#if defined(_WIN32) || defined(_WIN64)
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(), 0755);
#endif
    // Ignore errors (EEXIST is OK)
}

// ============================================================
// CONSTRUCTOR / DESTRUCTOR
// ============================================================

GraphDB::GraphDB(const std::string& db_dir)
    : pimpl_(nullptr) {
    ensure_directory(db_dir);
    pimpl_ = std::make_unique<Impl>(db_dir);
}

GraphDB::~GraphDB() {
    if (pimpl_) {
        // Auto-sync trước khi close
        sync();
    }
}

GraphDB::GraphDB(GraphDB&&) noexcept = default;
GraphDB& GraphDB::operator=(GraphDB&&) noexcept = default;

// ============================================================
// SCHEMA API
// ============================================================

NodeType GraphDB::defineNodeType(const std::string& type_name) {
    return pimpl_->schema_.registerNodeType(type_name);
}

EdgeType GraphDB::defineEdgeType(const std::string& type_name) {
    return pimpl_->schema_.registerEdgeType(type_name);
}

// ============================================================
// GRAPH API
// ============================================================

NodeID GraphDB::addNode(const std::string& node_name) {
    // 1. Kiểm tra node đã tồn tại chưa (via string pool)
    uint32_t existing_id = pimpl_->string_pool_.get_id(node_name);
    if (existing_id != 0) {
        return existing_id;  // Node đã tồn tại → trả ID cũ
    }

    // 2. Tạo ID mới trong string pool
    uint32_t str_id = pimpl_->string_pool_.get_or_create_id(node_name);

    // 3. Cấp phát NodeRecord trên mmap
    //    Đảm bảo NodeStore có đủ capacity cho str_id
    pimpl_->node_store_.ensureCapacity(str_id);

    // 4. Đồng bộ next_id nếu cần (string pool ID phải = node store slot)
    //    NodeStore.next_id có thể chưa bắt kịp str_id nếu là reopen
    auto* hdr = pimpl_->node_store_.getRecord(0);  // Dummy access to ensure mapping
    (void)hdr;

    // 5. Khởi tạo NodeRecord
    NodeRecord* rec = pimpl_->node_store_.getRecord(str_id);
    if (rec->flags == 0) {
        // Node chưa được khởi tạo
        rec->flags = 1;  // Active
        rec->first_edge_page = NULL_PAGE;
        rec->first_edge_slot = NULL_SLOT;
        rec->edge_count = 0;
        rec->node_type = 0;  // Chưa gán type (có thể gán riêng)
    }

    return str_id;
}

NodeID GraphDB::getNodeId(const std::string& node_name) {
    return pimpl_->string_pool_.get_id(node_name);
}

std::string GraphDB::getNodeName(NodeID id) const {
    return pimpl_->string_pool_.get_string(id);
}

bool GraphDB::addEdge(NodeID from_id, NodeID to_id, EdgeType edge_type,
                      const uint8_t* payload, uint8_t payload_size) {
    // Validate nodes exist
    pimpl_->node_store_.ensureCapacity(from_id);
    pimpl_->node_store_.ensureCapacity(to_id);

    // 1. Tạo GenericEdge
    GenericEdge edge(to_id, edge_type, payload, payload_size);

    // 2. Lấy chain head hiện tại từ NodeRecord
    NodeRecord* rec = pimpl_->node_store_.getRecord(from_id);
    uint32_t chain_page = rec->first_edge_page;
    uint16_t chain_slot = rec->first_edge_slot;

    // 3. Prepend edge vào linked list trên mmap
    pimpl_->page_manager_.prependEdge(chain_page, chain_slot, edge);

    // 4. Cập nhật NodeRecord (re-fetch vì prependEdge có thể grow → remap)
    rec = pimpl_->node_store_.getRecord(from_id);
    rec->first_edge_page = chain_page;
    rec->first_edge_slot = chain_slot;
    rec->edge_count++;

    // 5. Invalidate cache — edge list cũ đã stale
    pimpl_->edge_cache_.invalidate(from_id);

    return true;
}

const utils::MiniVector<GenericEdge>& GraphDB::getEdges(NodeID node_id) const {
    // 1. Try cache
    auto* cached = pimpl_->edge_cache_.lookup(node_id);
    if (cached) {
        return *cached;  // CACHE HIT
    }

    // 2. CACHE MISS — load from disk
    pimpl_->node_store_.ensureCapacity(node_id);
    const NodeRecord* rec = pimpl_->node_store_.getRecord(node_id);

    if (rec->flags == 0 || rec->first_edge_page == NULL_PAGE) {
        return Impl::emptyEdgeList();
    }

    // 3. Read edge chain from mmap
    utils::MiniVector<GenericEdge> edges;
    pimpl_->page_manager_.readEdgeChain(
        rec->first_edge_page, rec->first_edge_slot, edges);

    // 4. Insert into cache
    pimpl_->edge_cache_.insert(node_id, std::move(edges));

    // 5. Return from cache (pointer is now valid)
    auto* result = pimpl_->edge_cache_.lookup(node_id);
    if (result) {
        return *result;
    }

    // Fallback (shouldn't reach here)
    return Impl::emptyEdgeList();
}

// ============================================================
// STORAGE API
// ============================================================

void GraphDB::sync() {
    if (!pimpl_) return;
    pimpl_->node_store_.sync();
    pimpl_->page_manager_.sync();
    pimpl_->string_pool_.sync();
}

} // namespace graphlite
