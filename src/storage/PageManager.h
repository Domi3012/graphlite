/**
 * @file PageManager.h
 * @brief Quản lý cấp phát/thu hồi Page và Slot trên mmap file (edges.gldb).
 * @version 1.0
 *
 * Kiến trúc file `edges.gldb`:
 * ```
 * ┌──────────────────────────────────────────────────────┐
 * │ EdgeFileHeader (64B)                                  │
 * ├──────────────────────────────────────────────────────┤
 * │ Page 0 (4096B)                                        │
 * │   ├─ PageHeader (64B)                                 │
 * │   └─ DiskEdge[0..125] (126 × 32B = 4032B)            │
 * ├──────────────────────────────────────────────────────┤
 * │ Page 1 (4096B) ...                                    │
 * └──────────────────────────────────────────────────────┘
 * ```
 *
 * Cung cấp:
 * - Page allocation/deallocation: free-list ở cấp file (EdgeFileHeader.free_page_head)
 * - Slot allocation/deallocation: free-list ở cấp page (PageHeader.free_slot_head)
 * - O(1) getEdge(page_id, slot_id): pointer arithmetic trực tiếp trên mmap
 * - Auto-grow: file tự mở rộng khi hết pages
 *
 * Free-list hoạt động:
 * - Page free-list: EdgeFileHeader.free_page_head → Page.next_free_page → ... → NULL_SLOT
 * - Slot free-list: PageHeader.free_slot_head → DiskEdge.next_slot → ... → NULL_SLOT
 *   (Slot trống tái sử dụng trường next_slot của DiskEdge làm pointer)
 *
 * Dùng hybrid macro pattern (GRAPHLITE_IMPL_GUARD).
 */

#pragma once
#include "MmapFile.h"
#include <graphlite/MiniVector.h>
#include <graphlite/types.h>
#include <string>
#include <stdexcept>
#include <cstring>

namespace graphlite {
namespace internal {

/**
 * @class PageManager
 * @brief Quản lý vòng đời page/slot trong edge file.
 *
 * Sử dụng:
 * @code
 * PageManager mgr("./my_db");
 * uint16_t page = mgr.allocPage();          // Cấp phát page mới
 * uint16_t slot = mgr.allocSlot(page);      // Cấp phát slot trong page
 * DiskEdge* e = mgr.getEdge(page, slot);    // O(1) pointer access
 * e->target_node = 42;                       // Ghi trực tiếp lên mmap
 * mgr.freeSlot(page, slot);                  // Thu hồi slot
 * mgr.sync();
 * @endcode
 */
class PageManager {
private:
    MmapFile file_;

    // ==========================================
    // INTERNAL HELPERS
    // ==========================================

    /** @brief Con trỏ tới file header (đầu file). */
    EdgeFileHeader* fileHeader();
    const EdgeFileHeader* fileHeader() const;

    /** @brief Con trỏ tới PageHeader của page `page_id`. */
    PageHeader* pageHeader(uint16_t page_id);
    const PageHeader* pageHeader(uint16_t page_id) const;

    /**
     * @brief Tính byte offset tuyệt đối của page `page_id` trong file.
     * Formula: sizeof(EdgeFileHeader) + page_id × GLDB_PAGE_SIZE
     */
    size_t pageOffset(uint16_t page_id) const;

    /**
     * @brief Tính byte offset tuyệt đối của slot `slot_id` trong page `page_id`.
     * Formula: pageOffset(page_id) + sizeof(PageHeader) + slot_id × sizeof(DiskEdge)
     */
    size_t slotOffset(uint16_t page_id, uint16_t slot_id) const;

    /**
     * @brief Khởi tạo page mới: zero-fill, set slot_capacity, free_slot_head = NULL_SLOT.
     * @param page_id Page cần khởi tạo.
     */
    void initPage(uint16_t page_id);

    /**
     * @brief Mở rộng file thêm `count` pages mới.
     * Pages mới được thêm vào file-level free list.
     * @param count Số pages cần thêm.
     */
    void growPages(uint16_t count);

    /** @brief Magic bytes cho nhận dạng file. */
    static constexpr uint8_t MAGIC[4] = {'G', 'L', 'E', 'D'};
    static constexpr uint32_t FORMAT_VERSION = 1;

    /** @brief Số pages khởi tạo ban đầu. */
    static constexpr uint16_t DEFAULT_INITIAL_PAGES = 16;

    /** @brief Khi grow, thêm bao nhiêu pages mỗi lần. */
    static constexpr uint16_t GROW_PAGES = 32;

public:
    /**
     * @brief Mở/tạo PageManager.
     * @param db_dir Đường dẫn thư mục database. File `edges.gldb` sẽ nằm bên trong.
     *
     * Nếu file mới → khởi tạo header + cấp phát DEFAULT_INITIAL_PAGES pages.
     * Nếu file cũ → verify magic + version, đọc metadata.
     */
    explicit PageManager(const std::string& db_dir);

    ~PageManager() = default;

    // Chặn copy, cho phép move
    PageManager(const PageManager&) = delete;
    PageManager& operator=(const PageManager&) = delete;
    PageManager(PageManager&&) noexcept = default;
    PageManager& operator=(PageManager&&) noexcept = default;

    // ==========================================
    // PAGE-LEVEL OPERATIONS
    // ==========================================

    /**
     * @brief Cấp phát 1 page từ free list (hoặc grow nếu hết).
     * @return Page ID mới.
     */
    uint16_t allocPage();

    /**
     * @brief Trả page về file-level free list.
     * @param page_id Page cần thu hồi.
     * @note Page bị reset — mọi edges trong page sẽ mất.
     */
    void freePage(uint16_t page_id);

    // ==========================================
    // SLOT-LEVEL OPERATIONS
    // ==========================================

    /**
     * @brief Cấp phát 1 slot trong page.
     * @param page_id Page cần cấp slot.
     * @return Slot ID mới trong page đó.
     *
     * Ưu tiên: (1) lấy từ free list → (2) append slot mới.
     * Nếu page đầy → trả NULL_SLOT.
     */
    uint16_t allocSlot(uint16_t page_id);

    /**
     * @brief Thu hồi slot, đưa về page-level free list.
     * @param page_id Page chứa slot.
     * @param slot_id Slot cần thu hồi.
     */
    void freeSlot(uint16_t page_id, uint16_t slot_id);

    /**
     * @brief Kiểm tra page còn slot trống không.
     * @param page_id Page cần kiểm tra.
     * @return true nếu còn slot (free list hoặc chưa dùng hết capacity).
     */
    bool hasAvailableSlot(uint16_t page_id) const;

    // ==========================================
    // EDGE ACCESS — O(1) pointer arithmetic
    // ==========================================

    /**
     * @brief Truy cập DiskEdge tại (page_id, slot_id) — zero-copy.
     * @return Con trỏ trực tiếp vào mmap region.
     *
     * @warning Con trỏ INVALIDATE sau allocPage()/growPages(). Luôn lấy lại sau grow.
     */
    DiskEdge* getEdge(uint16_t page_id, uint16_t slot_id);
    const DiskEdge* getEdge(uint16_t page_id, uint16_t slot_id) const;

    // ==========================================
    // LINKED LIST OPERATIONS
    // ==========================================

    /**
     * @brief Đọc toàn bộ linked list edges từ (start_page, start_slot)
     * @param start_page Page bắt đầu
     * @param start_slot Slot bắt đầu
     * @param is_out_chain Nếu true, duyệt theo next_page/next_slot. Nếu false, duyệt theo next_in_page/next_in_slot.
     * @param out_edges Vector chứa kết quả trả về
     */
    void readEdgeChain(uint32_t start_page, uint16_t start_slot, bool is_out_chain, utils::MiniVector<GenericEdge>& out_edges) const;

    /**
     * @brief Chèn edge mới vào ĐẦU danh sách liên kết của cả Out-edges và In-edges.
     * @param out_chain_page [in,out] Page của out-edge đầu tiên. Sẽ được cập nhật.
     * @param out_chain_slot [in,out] Slot của out-edge đầu tiên. Sẽ được cập nhật.
     * @param in_chain_page  [in,out] Page của in-edge đầu tiên. Sẽ được cập nhật.
     * @param in_chain_slot  [in,out] Slot của in-edge đầu tiên. Sẽ được cập nhật.
     * @param edge Dữ liệu edge cần chèn.
     */
    void prependBidirectionalEdge(
        uint32_t& out_chain_page, uint16_t& out_chain_slot,
        uint32_t& in_chain_page,  uint16_t& in_chain_slot,
        const GenericEdge& edge);


    // ==========================================
    // METADATA
    // ==========================================

    /** @brief Tổng số pages đã cấp phát (kể cả đang free). */
    uint32_t pageCount() const;

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
