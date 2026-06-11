/**
 * @file NodeStore.h
 * @brief Flat array lưu trữ NodeRecord trên mmap (nodes.gldb).
 * @version 1.0
 *
 * Kiến trúc:
 * - File `nodes.gldb` = NodeFileHeader(64B) + NodeRecord[](mỗi cái 32B)
 * - Truy cập O(1): getRecord(id) = pointer arithmetic thuần túy
 * - Auto-grow: ensureCapacity() mở rộng file + remap khi cần
 * - Persistence: Dữ liệu nằm trên mmap → tắt app → mở lại → nguyên vẹn
 *
 * First-open vs Reopen:
 * - Nếu file mới (magic chưa đúng): ghi header khởi tạo
 * - Nếu file đã tồn tại (magic đúng): đọc header, verify version
 *
 * Dùng hybrid macro pattern (GRAPHLITE_IMPL_GUARD).
 */

#pragma once
#include "MmapFile.h"
#include "../types.h"
#include <string>
#include <stdexcept>
#include <cstring>

namespace graphlite {
namespace internal {

/**
 * @class NodeStore
 * @brief Quản lý vòng đời và truy cập NodeRecord trên mmap file.
 *
 * Sử dụng:
 * @code
 * NodeStore store("./my_db");           // Mở/tạo nodes.gldb trong thư mục
 * NodeID id = store.allocNodeId();       // Cấp ID mới
 * store.ensureCapacity(id);              // Đảm bảo file đủ lớn
 * NodeRecord* rec = store.getRecord(id); // O(1) access
 * rec->flags = 1;                        // Ghi trực tiếp lên mmap
 * store.sync();                          // Flush xuống đĩa
 * @endcode
 */
class NodeStore {
private:
    MmapFile file_;

    /** @brief Số NodeRecord tối đa mà file hiện tại chứa được. */
    uint32_t record_capacity_;

    /** @brief Tính lại record_capacity_ từ file capacity. */
    void updateRecordCapacity();

    /** @brief Trả con trỏ tới header (đầu file). */
    NodeFileHeader* header();
    const NodeFileHeader* header() const;

    /** @brief Magic bytes cho nhận dạng file. */
    static constexpr uint8_t MAGIC[4] = {'G', 'L', 'N', 'D'};
    static constexpr uint32_t FORMAT_VERSION = 1;

    /** @brief Kích thước khởi tạo mặc định: 64B header + 4096 records × 32B = ~128KB. */
    static constexpr size_t DEFAULT_INITIAL_RECORDS = 4096;
    static constexpr size_t DEFAULT_INITIAL_SIZE =
        sizeof(NodeFileHeader) + DEFAULT_INITIAL_RECORDS * sizeof(NodeRecord);

public:
    /**
     * @brief Mở/tạo NodeStore.
     * @param db_dir Đường dẫn thư mục database. File `nodes.gldb` sẽ nằm bên trong.
     *
     * Nếu file chưa tồn tại → tạo mới, ghi header khởi tạo.
     * Nếu file đã tồn tại → verify magic + version, đọc metadata.
     */
    explicit NodeStore(const std::string& db_dir);

    ~NodeStore() = default;

    // Chặn copy
    NodeStore(const NodeStore&) = delete;
    NodeStore& operator=(const NodeStore&) = delete;

    // Cho phép move
    NodeStore(NodeStore&&) noexcept = default;
    NodeStore& operator=(NodeStore&&) noexcept = default;

    // ==========================================
    // NODE MANAGEMENT
    // ==========================================

    /**
     * @brief Cấp phát NodeID mới (auto-increment).
     * @return NodeID mới, tự tăng từ header.next_id.
     * @note Tự gọi ensureCapacity() nếu cần.
     */
    NodeID allocNodeId();

    /**
     * @brief Đảm bảo file đủ lớn để chứa NodeRecord cho `id`.
     * @param id NodeID cần đảm bảo capacity.
     *
     * Nếu `id >= record_capacity_`, file sẽ được grow.
     * Chiến lược grow: capacity × 2 (geometric) hoặc vừa đủ, tùy cái nào lớn hơn.
     */
    void ensureCapacity(NodeID id);

    /**
     * @brief Truy cập NodeRecord — O(1) pointer arithmetic.
     * @param id NodeID cần truy cập.
     * @return Con trỏ trực tiếp vào mmap region (zero-copy).
     *
     * @note Con trỏ trả về CÓ THỂ BỊ INVALIDATE nếu gọi ensureCapacity()
     *       hoặc allocNodeId() sau đó (do mmap remap). Luôn lấy lại con trỏ
     *       sau bất kỳ thao tác có thể grow file.
     *
     * @warning Không kiểm tra bounds — caller phải đảm bảo id < record_capacity_.
     */
    NodeRecord* getRecord(NodeID id);
    const NodeRecord* getRecord(NodeID id) const;

    // ==========================================
    // METADATA
    // ==========================================

    /** @brief Số đỉnh đang active. */
    uint32_t nodeCount() const;

    /** @brief ID kế tiếp sẽ được cấp phát. */
    uint32_t nextId() const;

    /** @brief Số NodeRecord tối đa mà file hiện tại chứa được (trước khi cần grow). */
    uint32_t recordCapacity() const { return record_capacity_; }

    // ==========================================
    // PERSISTENCE
    // ==========================================

    /** @brief Flush dirty pages xuống ổ đĩa. */
    void sync();
};

// ============================================================
// IMPLEMENTATION
// ============================================================

#ifdef GRAPHLITE_IMPL_GUARD

// --- Private helpers ---

GRAPHLITE_FUNC void NodeStore::updateRecordCapacity() {
    if (file_.capacity() <= sizeof(NodeFileHeader)) {
        record_capacity_ = 0;
    } else {
        record_capacity_ = static_cast<uint32_t>(
            (file_.capacity() - sizeof(NodeFileHeader)) / sizeof(NodeRecord));
    }
}

GRAPHLITE_FUNC NodeFileHeader* NodeStore::header() {
    return file_.as<NodeFileHeader>();
}

GRAPHLITE_FUNC const NodeFileHeader* NodeStore::header() const {
    return file_.as<NodeFileHeader>();
}

// --- Constructor ---

GRAPHLITE_FUNC NodeStore::NodeStore(const std::string& db_dir)
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

GRAPHLITE_FUNC NodeID NodeStore::allocNodeId() {
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

GRAPHLITE_FUNC void NodeStore::ensureCapacity(NodeID id) {
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

GRAPHLITE_FUNC NodeRecord* NodeStore::getRecord(NodeID id) {
    return file_.at<NodeRecord>(sizeof(NodeFileHeader) + 
                                static_cast<size_t>(id) * sizeof(NodeRecord));
}

GRAPHLITE_FUNC const NodeRecord* NodeStore::getRecord(NodeID id) const {
    return file_.at<NodeRecord>(sizeof(NodeFileHeader) + 
                                static_cast<size_t>(id) * sizeof(NodeRecord));
}

// --- Metadata ---

GRAPHLITE_FUNC uint32_t NodeStore::nodeCount() const {
    return header()->node_count;
}

GRAPHLITE_FUNC uint32_t NodeStore::nextId() const {
    return header()->next_id;
}

// --- Persistence ---

GRAPHLITE_FUNC void NodeStore::sync() {
    file_.sync();
}

#endif // GRAPHLITE_IMPL_GUARD

} // namespace internal
} // namespace graphlite
