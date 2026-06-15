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
#include <graphlite/types.h>
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

    /** @brief Tăng số lượng đỉnh. */
    void incrementNodeCount();

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

} // namespace internal
} // namespace graphlite
