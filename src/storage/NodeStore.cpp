#include "NodeStore.h"
#include <cstring>
#include <stdexcept>

namespace graphlite {
namespace internal {

// --- Private helpers ---

void NodeStore::updateRecordCapacity() {
    if (file_.capacity() <= sizeof(NodeFileHeader)) {
        record_capacity_ = 0;
    } else {
        record_capacity_ = static_cast<uint32_t>(
            (file_.capacity() - sizeof(NodeFileHeader)) / sizeof(NodeRecord));
    }
}

NodeFileHeader* NodeStore::header() {
    return file_.as<NodeFileHeader>();
}

const NodeFileHeader* NodeStore::header() const {
    return file_.as<NodeFileHeader>();
}

// --- Constructor ---

NodeStore::NodeStore(const std::string& db_dir)
    : file_(db_dir + "/nodes.gldb", DEFAULT_INITIAL_SIZE),
      record_capacity_(0) {

    auto* hdr = header();

    // Kiểm tra magic → first-open hay reopen?
    bool is_new = (hdr->magic[0] != MAGIC[0] ||
                   hdr->magic[1] != MAGIC[1] ||
                   hdr->magic[2] != MAGIC[2] ||
                   hdr->magic[3] != MAGIC[3]);

    if (is_new) {
        // === FIRST OPEN: Khởi tạo header ===
        std::memset(hdr, 0, sizeof(NodeFileHeader));
        std::memcpy(hdr->magic, MAGIC, 4);
        hdr->version    = FORMAT_VERSION;
        hdr->node_count = 0;
        hdr->next_id    = 1;  // ID bắt đầu từ 1 (0 = reserved/invalid)
    } else {
        // === REOPEN: Verify version ===
        if (hdr->version != FORMAT_VERSION) {
            throw std::runtime_error(
                "GraphLite: nodes.gldb format version mismatch. "
                "Expected " + std::to_string(FORMAT_VERSION) +
                ", got " + std::to_string(hdr->version));
        }
    }

    updateRecordCapacity();
}

// --- Node Management ---

NodeID NodeStore::allocNodeId() {
    auto* hdr = header();
    NodeID new_id = hdr->next_id;
    hdr->next_id++;
    hdr->node_count++;

    // Đảm bảo file đủ lớn
    ensureCapacity(new_id);

    // Khởi tạo NodeRecord mới — zero-fill + set sentinel values
    NodeRecord* rec = getRecord(new_id);
    std::memset(rec, 0, sizeof(NodeRecord));
    rec->first_edge_page = NULL_PAGE;
    rec->first_edge_slot = NULL_SLOT;
    rec->edge_count = 0;
    rec->flags = 0;  // Chưa active — caller sẽ set flags = 1 khi addNode()

    return new_id;
}

void NodeStore::ensureCapacity(NodeID id) {
    if (id < record_capacity_) return;

    // Tính capacity mới: geometric × 2 hoặc vừa đủ, tùy cái lớn hơn
    uint32_t new_capacity = record_capacity_;
    if (new_capacity == 0) new_capacity = DEFAULT_INITIAL_RECORDS;
    while (new_capacity <= id) {
        new_capacity *= 2;
    }

    size_t new_size = sizeof(NodeFileHeader) + 
                      static_cast<size_t>(new_capacity) * sizeof(NodeRecord);
    file_.grow(new_size);
    updateRecordCapacity();
}

NodeRecord* NodeStore::getRecord(NodeID id) {
    return file_.at<NodeRecord>(sizeof(NodeFileHeader) + 
                                static_cast<size_t>(id) * sizeof(NodeRecord));
}

const NodeRecord* NodeStore::getRecord(NodeID id) const {
    return file_.at<NodeRecord>(sizeof(NodeFileHeader) + 
                                static_cast<size_t>(id) * sizeof(NodeRecord));
}

// --- Metadata ---

uint32_t NodeStore::nodeCount() const {
    return header()->node_count;
}

uint32_t NodeStore::nextId() const {
    return header()->next_id;
}

// --- Persistence ---

void NodeStore::sync() {
    file_.sync();
}

} // namespace internal
} // namespace graphlite
