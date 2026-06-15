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
    internal::ClockCache<utils::MiniVector<GenericEdge>> out_edge_cache_;
    internal::ClockCache<utils::MiniVector<GenericEdge>> in_edge_cache_;

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
          out_edge_cache_(8192),
          in_edge_cache_(8192) {}

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
        rec->first_in_edge_page = NULL_PAGE;
        rec->first_in_edge_slot = NULL_SLOT;
        rec->in_edge_count = 0;
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

size_t GraphDB::getNodeCount() const {
    return pimpl_->string_pool_.get_id_count();
}

bool GraphDB::addEdge(NodeID from_id, NodeID to_id, EdgeType edge_type,
                      const uint8_t* payload, uint8_t payload_size) {
    // Validate nodes exist
    pimpl_->node_store_.ensureCapacity(from_id);
    pimpl_->node_store_.ensureCapacity(to_id);

    // 1. Tạo GenericEdge
    GenericEdge edge(from_id, to_id, edge_type, payload, payload_size);

    // 2. Lấy out-chain head hiện tại từ NodeRecord của 'from'
    NodeRecord* from_rec = pimpl_->node_store_.getRecord(from_id);
    uint32_t out_chain_page = from_rec->first_edge_page;
    uint16_t out_chain_slot = from_rec->first_edge_slot;

    // 3. Lấy in-chain head hiện tại từ NodeRecord của 'to'
    NodeRecord* to_rec = pimpl_->node_store_.getRecord(to_id);
    uint32_t in_chain_page = to_rec->first_in_edge_page;
    uint16_t in_chain_slot = to_rec->first_in_edge_slot;

    // 4. Prepend edge vào cả 2 linked lists trên mmap
    pimpl_->page_manager_.prependBidirectionalEdge(
        out_chain_page, out_chain_slot,
        in_chain_page, in_chain_slot,
        edge);

    // 5. Cập nhật NodeRecord của 'from' (re-fetch vì mmap có thể remap)
    from_rec = pimpl_->node_store_.getRecord(from_id);
    from_rec->first_edge_page = out_chain_page;
    from_rec->first_edge_slot = out_chain_slot;
    from_rec->edge_count++;

    // 6. Cập nhật NodeRecord của 'to'
    to_rec = pimpl_->node_store_.getRecord(to_id);
    to_rec->first_in_edge_page = in_chain_page;
    to_rec->first_in_edge_slot = in_chain_slot;
    to_rec->in_edge_count++;

    // 7. Invalidate cache — edge list cũ đã stale
    pimpl_->out_edge_cache_.invalidate(from_id);
    pimpl_->in_edge_cache_.invalidate(to_id);

    return true;
}

const utils::MiniVector<GenericEdge>& GraphDB::getOutEdges(NodeID node_id) const {
    // 1. Try cache
    auto* cached = pimpl_->out_edge_cache_.lookup(node_id);
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
        rec->first_edge_page, rec->first_edge_slot, true, edges);

    // 4. Insert into cache
    pimpl_->out_edge_cache_.insert(node_id, std::move(edges));

    // 5. Return from cache (pointer is now valid)
    auto* result = pimpl_->out_edge_cache_.lookup(node_id);
    if (result) {
        return *result;
    }

    // Fallback (shouldn't reach here)
    return Impl::emptyEdgeList();
}

const utils::MiniVector<GenericEdge>& GraphDB::getInEdges(NodeID node_id) const {
    // 1. Try cache
    auto* cached = pimpl_->in_edge_cache_.lookup(node_id);
    if (cached) {
        return *cached;  // CACHE HIT
    }

    // 2. CACHE MISS — load from disk
    pimpl_->node_store_.ensureCapacity(node_id);
    const NodeRecord* rec = pimpl_->node_store_.getRecord(node_id);

    if (rec->flags == 0 || rec->first_in_edge_page == NULL_PAGE) {
        return Impl::emptyEdgeList();
    }

    // 3. Read edge chain from mmap
    utils::MiniVector<GenericEdge> edges;
    pimpl_->page_manager_.readEdgeChain(
        rec->first_in_edge_page, rec->first_in_edge_slot, false, edges);

    // 4. Insert into cache
    pimpl_->in_edge_cache_.insert(node_id, std::move(edges));

    // 5. Return from cache (pointer is now valid)
    auto* result = pimpl_->in_edge_cache_.lookup(node_id);
    if (result) {
        return *result;
    }

    // Fallback (shouldn't reach here)
    return Impl::emptyEdgeList();
}

// ============================================================
// BATCH API
// ============================================================

void GraphDB::insertEdgesBatch(const utils::MiniVector<EdgeInsertData>& edges) {
    if (edges.empty()) return;

    // 0. Đảm bảo sức chứa trước
    NodeID max_id = 0;
    for (size_t i = 0; i < edges.size(); ++i) {
        if (edges[i].from > max_id) max_id = edges[i].from;
        if (edges[i].to > max_id) max_id = edges[i].to;
    }
    pimpl_->node_store_.ensureCapacity(max_id);

    // 1. Đăng ký tất cả các Node (để tự động tăng node_count và string pool, cũng như initialize record)
    for (size_t i = 0; i < edges.size(); ++i) {
        // Tạm thời hack: Nếu node chưa được set flag = 1, ta khởi tạo nó
        NodeRecord* from_rec = pimpl_->node_store_.getRecord(edges[i].from);
        if (from_rec->flags == 0) {
            from_rec->flags = 1;
            from_rec->first_edge_page = NULL_PAGE;
            from_rec->first_edge_slot = NULL_SLOT;
            from_rec->first_in_edge_page = NULL_PAGE;
            from_rec->first_in_edge_slot = NULL_SLOT;
            from_rec->edge_count = 0;
            from_rec->in_edge_count = 0;
            pimpl_->node_store_.incrementNodeCount();
            
            // Dummy string cho ID này để string pool next_id_ theo kịp
            pimpl_->string_pool_.get_or_create_id(std::to_string(edges[i].from));
        }
        
        NodeRecord* to_rec = pimpl_->node_store_.getRecord(edges[i].to);
        if (to_rec->flags == 0) {
            to_rec->flags = 1;
            to_rec->first_edge_page = NULL_PAGE;
            to_rec->first_edge_slot = NULL_SLOT;
            to_rec->first_in_edge_page = NULL_PAGE;
            to_rec->first_in_edge_slot = NULL_SLOT;
            to_rec->edge_count = 0;
            to_rec->in_edge_count = 0;
            pimpl_->node_store_.incrementNodeCount();
            
            pimpl_->string_pool_.get_or_create_id(std::to_string(edges[i].to));
        }
    }

    // 2. Chèn tất cả
    for (size_t i = 0; i < edges.size(); ++i) {
        const auto& item = edges[i];
        GenericEdge edge(item.from, item.to, item.type, item.payload, item.payload_size);

        NodeRecord* from_rec = pimpl_->node_store_.getRecord(item.from);
        uint32_t out_chain_page = from_rec->first_edge_page;
        uint16_t out_chain_slot = from_rec->first_edge_slot;

        NodeRecord* to_rec = pimpl_->node_store_.getRecord(item.to);
        uint32_t in_chain_page = to_rec->first_in_edge_page;
        uint16_t in_chain_slot = to_rec->first_in_edge_slot;

        pimpl_->page_manager_.prependBidirectionalEdge(
            out_chain_page, out_chain_slot,
            in_chain_page, in_chain_slot,
            edge);

        from_rec = pimpl_->node_store_.getRecord(item.from);
        from_rec->first_edge_page = out_chain_page;
        from_rec->first_edge_slot = out_chain_slot;
        from_rec->edge_count++;

        to_rec = pimpl_->node_store_.getRecord(item.to);
        to_rec->first_in_edge_page = in_chain_page;
        to_rec->first_in_edge_slot = in_chain_slot;
        to_rec->in_edge_count++;
    }

    // 3. Clear cache 1 lần duy nhất
    pimpl_->out_edge_cache_.clear();
    pimpl_->in_edge_cache_.clear();
}

// ============================================================
// STORAGE & MEMORY API
// ============================================================

void GraphDB::sync() {
    if (!pimpl_) return;
    pimpl_->node_store_.sync();
    pimpl_->page_manager_.sync();
    pimpl_->string_pool_.sync();
}

void GraphDB::clearGraph() {
    if (!pimpl_) return;
    std::string dir = pimpl_->db_dir_;
    
    // Hủy Impl hiện tại để giải phóng Mmap và close file handles
    pimpl_.reset();
    
    // Xoá các file trên đĩa
    std::remove((dir + "/nodes.gldb").c_str());
    std::remove((dir + "/edges.gldb").c_str());
    std::remove((dir + "/strings.gldb").c_str());
    
    // Tái tạo lại Impl
    ensure_directory(dir);
    pimpl_ = std::make_unique<Impl>(dir);
}

size_t GraphDB::getMemoryUsage() const {
    size_t total = 0;
    struct stat st;
    std::string dir = pimpl_->db_dir_;
    
    if (stat((dir + "/nodes.gldb").c_str(), &st) == 0) total += st.st_size;
    if (stat((dir + "/edges.gldb").c_str(), &st) == 0) total += st.st_size;
    if (stat((dir + "/strings.gldb").c_str(), &st) == 0) total += st.st_size;
    
    // Cộng thêm kích thước RAM dự tính của Cache (metadata + buffers)
    total += 8192 * sizeof(utils::MiniVector<GenericEdge>) * 2; // out_cache, in_cache
    
    return total;
}

} // namespace graphlite
