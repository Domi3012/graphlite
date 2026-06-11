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
#include "MiniVector.h"
#include "../types.h"
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
     * @param out_edges Vector chứa kết quả trả về
     */
    void readEdgeChain(uint32_t start_page, uint16_t start_slot, utils::MiniVector<GenericEdge>& out_edges) const;

    /**
     * @brief Chèn edge mới vào ĐẦU danh sách liên kết.
     * @param chain_page [in,out] Page của edge đầu tiên. Sẽ được cập nhật.
     * @param chain_slot [in,out] Slot của edge đầu tiên. Sẽ được cập nhật.
     * @param edge Dữ liệu edge cần chèn.
     */
    void prependEdge(uint32_t& chain_page, uint16_t& chain_slot, const GenericEdge& edge);


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

#ifdef GRAPHLITE_IMPL_GUARD

// --- Private helpers ---

GRAPHLITE_FUNC EdgeFileHeader* PageManager::fileHeader() {
    return file_.as<EdgeFileHeader>();
}

GRAPHLITE_FUNC const EdgeFileHeader* PageManager::fileHeader() const {
    return file_.as<EdgeFileHeader>();
}

GRAPHLITE_FUNC size_t PageManager::pageOffset(uint16_t page_id) const {
    return sizeof(EdgeFileHeader) +
           static_cast<size_t>(page_id) * GLDB_PAGE_SIZE;
}

GRAPHLITE_FUNC size_t PageManager::slotOffset(uint16_t page_id, uint16_t slot_id) const {
    return pageOffset(page_id) + sizeof(PageHeader) +
           static_cast<size_t>(slot_id) * sizeof(DiskEdge);
}

GRAPHLITE_FUNC PageHeader* PageManager::pageHeader(uint16_t page_id) {
    return file_.at<PageHeader>(pageOffset(page_id));
}

GRAPHLITE_FUNC const PageHeader* PageManager::pageHeader(uint16_t page_id) const {
    return file_.at<PageHeader>(pageOffset(page_id));
}

GRAPHLITE_FUNC void PageManager::initPage(uint16_t page_id) {
    // Zero-fill toàn bộ page (header + slots)
    uint8_t* page_start = file_.at<uint8_t>(pageOffset(page_id));
    std::memset(page_start, 0, GLDB_PAGE_SIZE);

    // Khởi tạo PageHeader
    PageHeader* ph = pageHeader(page_id);
    ph->slot_count    = 0;
    ph->slot_capacity = EDGES_PER_PAGE;
    ph->free_slot_head = NULL_SLOT;    // Chưa có slot nào bị free
    ph->next_free_page = NULL_SLOT;    // Chưa link vào free list
}

GRAPHLITE_FUNC void PageManager::growPages(uint16_t count) {
    auto* hdr = fileHeader();
    uint32_t old_count = hdr->page_count;
    uint32_t new_count = old_count + count;

    // Tính kích thước file mới
    size_t new_file_size = sizeof(EdgeFileHeader) +
                           static_cast<size_t>(new_count) * GLDB_PAGE_SIZE;
    file_.grow(new_file_size);

    // SAU grow, phải lấy lại pointer vì mmap có thể remap
    hdr = fileHeader();
    hdr->page_count = new_count;

    // Khởi tạo từng page mới và thêm vào file-level free list
    for (uint32_t i = old_count; i < new_count; ++i) {
        uint16_t pid = static_cast<uint16_t>(i);
        initPage(pid);

        // Đẩy page mới vào đầu free list
        PageHeader* ph = pageHeader(pid);
        ph->next_free_page = hdr->free_page_head;
        hdr->free_page_head = pid;
    }
}

// --- Constructor ---

GRAPHLITE_FUNC PageManager::PageManager(const std::string& db_dir)
    : file_(db_dir + "/edges.gldb",
            sizeof(EdgeFileHeader) +
            static_cast<size_t>(DEFAULT_INITIAL_PAGES) * GLDB_PAGE_SIZE) {

    auto* hdr = fileHeader();

    bool is_new = (hdr->magic[0] != MAGIC[0] ||
                   hdr->magic[1] != MAGIC[1] ||
                   hdr->magic[2] != MAGIC[2] ||
                   hdr->magic[3] != MAGIC[3]);

    if (is_new) {
        // === FIRST OPEN ===
        std::memset(hdr, 0, sizeof(EdgeFileHeader));
        std::memcpy(hdr->magic, MAGIC, 4);
        hdr->version        = FORMAT_VERSION;
        hdr->page_size      = GLDB_PAGE_SIZE;
        hdr->page_count     = 0;
        hdr->free_page_head = NULL_SLOT;

        // Cấp phát batch pages ban đầu
        growPages(DEFAULT_INITIAL_PAGES);
    } else {
        // === REOPEN ===
        if (hdr->version != FORMAT_VERSION) {
            throw std::runtime_error(
                "GraphLite: edges.gldb format version mismatch. "
                "Expected " + std::to_string(FORMAT_VERSION) +
                ", got " + std::to_string(hdr->version));
        }
    }
}

// --- Page-level operations ---

GRAPHLITE_FUNC uint16_t PageManager::allocPage() {
    auto* hdr = fileHeader();

    // Nếu free list rỗng → grow thêm pages
    if (hdr->free_page_head == NULL_SLOT) {
        growPages(GROW_PAGES);
        hdr = fileHeader();  // Re-fetch sau grow
    }

    // Pop page đầu free list
    uint16_t page_id = hdr->free_page_head;
    PageHeader* ph = pageHeader(page_id);
    hdr->free_page_head = ph->next_free_page;

    // Reset page cho sử dụng mới
    initPage(page_id);

    return page_id;
}

GRAPHLITE_FUNC void PageManager::freePage(uint16_t page_id) {
    auto* hdr = fileHeader();

    // Reset page
    initPage(page_id);

    // Push vào đầu free list
    PageHeader* ph = pageHeader(page_id);
    ph->next_free_page = hdr->free_page_head;
    hdr->free_page_head = page_id;
}

// --- Slot-level operations ---

GRAPHLITE_FUNC uint16_t PageManager::allocSlot(uint16_t page_id) {
    PageHeader* ph = pageHeader(page_id);

    // Ưu tiên 1: Lấy từ slot free list (slot đã bị free trước đó)
    if (ph->free_slot_head != NULL_SLOT) {
        uint16_t slot_id = ph->free_slot_head;
        DiskEdge* edge = getEdge(page_id, slot_id);

        // Pop khỏi free list: next pointer nằm trong DiskEdge.next_slot
        ph->free_slot_head = edge->next_slot;

        // Zero-fill slot trước khi trả
        std::memset(edge, 0, sizeof(DiskEdge));

        ph->slot_count++;
        return slot_id;
    }

    // Ưu tiên 2: Append slot mới (nếu chưa dùng hết capacity)
    uint16_t used_total = ph->slot_count;
    // Tính tổng slots đã từng alloc (count hiện tại + slots trong free list = high water mark)
    // Vì free list đã rỗng ở đây, slot_count chính là high water mark
    if (used_total < ph->slot_capacity) {
        uint16_t slot_id = used_total;
        DiskEdge* edge = getEdge(page_id, slot_id);
        std::memset(edge, 0, sizeof(DiskEdge));
        ph->slot_count++;
        return slot_id;
    }

    // Page đầy
    return NULL_SLOT;
}

GRAPHLITE_FUNC void PageManager::freeSlot(uint16_t page_id, uint16_t slot_id) {
    PageHeader* ph = pageHeader(page_id);
    DiskEdge* edge = getEdge(page_id, slot_id);

    // Zero-fill slot
    std::memset(edge, 0, sizeof(DiskEdge));

    // Push vào đầu slot free list
    // Tái sử dụng DiskEdge.next_slot làm free-list pointer
    edge->next_slot = ph->free_slot_head;
    edge->next_page = NULL_SLOT;  // Marker: slot này trống
    ph->free_slot_head = slot_id;

    ph->slot_count--;
}

GRAPHLITE_FUNC bool PageManager::hasAvailableSlot(uint16_t page_id) const {
    const PageHeader* ph = pageHeader(page_id);
    // Có slot trong free list, HOẶC chưa dùng hết capacity
    return (ph->free_slot_head != NULL_SLOT) ||
           (ph->slot_count < ph->slot_capacity);
}

// --- Edge access ---

GRAPHLITE_FUNC DiskEdge* PageManager::getEdge(uint16_t page_id, uint16_t slot_id) {
    return file_.at<DiskEdge>(slotOffset(page_id, slot_id));
}

GRAPHLITE_FUNC const DiskEdge* PageManager::getEdge(uint16_t page_id, uint16_t slot_id) const {
    return file_.at<DiskEdge>(slotOffset(page_id, slot_id));
}

// --- Metadata ---

GRAPHLITE_FUNC uint32_t PageManager::pageCount() const {
    return fileHeader()->page_count;
}

// --- Linked List Operations ---

GRAPHLITE_FUNC void PageManager::readEdgeChain(uint32_t start_page, uint16_t start_slot, utils::MiniVector<GenericEdge>& out_edges) const {
    uint32_t curr_page = start_page;
    uint16_t curr_slot = start_slot;

    while (curr_page != NULL_PAGE && curr_slot != NULL_SLOT) {
        // Safe cast vì edges.gldb hỗ trợ tối đa 65535 pages
        const DiskEdge* disk_edge = getEdge(static_cast<uint16_t>(curr_page), curr_slot);
        
        // Chuyển đổi DiskEdge → GenericEdge
        GenericEdge mem_edge;
        mem_edge.target_node = disk_edge->target_node;
        mem_edge.edge_type = disk_edge->edge_type;
        std::memcpy(mem_edge.payload, disk_edge->payload, MAX_PAYLOAD_SIZE);
        
        out_edges.push_back(std::move(mem_edge));

        // Nhảy sang node kế tiếp
        curr_page = disk_edge->next_page;
        curr_slot = disk_edge->next_slot;
    }
}

GRAPHLITE_FUNC void PageManager::prependEdge(uint32_t& chain_page, uint16_t& chain_slot, const GenericEdge& edge) {
    // 1. Cấp phát page/slot mới
    // Ưu tiên page hiện tại (chain_page) nếu còn slot trống để giảm cross-page fragmentation
    uint32_t target_page = NULL_PAGE;
    
    if (chain_page != NULL_PAGE && chain_page < 65536) {
        if (hasAvailableSlot(static_cast<uint16_t>(chain_page))) {
            target_page = static_cast<uint16_t>(chain_page);
        }
    }
    
    if (target_page == NULL_PAGE) {
        target_page = allocPage(); // Lấy page mới (hoặc từ free list)
    }
    
    uint16_t target_page_16 = static_cast<uint16_t>(target_page);
    uint16_t new_slot = allocSlot(target_page_16);
    
    // 2. Ghi dữ liệu edge mới
    DiskEdge* disk_edge = getEdge(target_page_16, new_slot);
    disk_edge->target_node = edge.target_node;
    disk_edge->edge_type = edge.edge_type;
    std::memcpy(disk_edge->payload, edge.payload, MAX_PAYLOAD_SIZE);
    
    // 3. Liên kết với chuỗi cũ (prepend)
    if (chain_page == NULL_PAGE) {
        disk_edge->next_page = NULL_SLOT;
        disk_edge->next_slot = NULL_SLOT;
    } else {
        disk_edge->next_page = static_cast<uint16_t>(chain_page);
        disk_edge->next_slot = chain_slot;
    }
    
    // 4. Cập nhật con trỏ head
    chain_page = target_page;
    chain_slot = new_slot;
}

// --- Persistence ---

GRAPHLITE_FUNC void PageManager::sync() {
    file_.sync();
}

#endif // GRAPHLITE_IMPL_GUARD

} // namespace internal
} // namespace graphlite
