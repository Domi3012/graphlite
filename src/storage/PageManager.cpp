#include "PageManager.h"
#include <cstring>
#include <stdexcept>

namespace graphlite {
namespace internal {

// --- Private helpers ---

EdgeFileHeader* PageManager::fileHeader() {
    return file_.as<EdgeFileHeader>();
}

const EdgeFileHeader* PageManager::fileHeader() const {
    return file_.as<EdgeFileHeader>();
}

size_t PageManager::pageOffset(uint16_t page_id) const {
    return sizeof(EdgeFileHeader) +
           static_cast<size_t>(page_id) * GLDB_PAGE_SIZE;
}

size_t PageManager::slotOffset(uint16_t page_id, uint16_t slot_id) const {
    return pageOffset(page_id) + sizeof(PageHeader) +
           static_cast<size_t>(slot_id) * sizeof(DiskEdge);
}

PageHeader* PageManager::pageHeader(uint16_t page_id) {
    return file_.at<PageHeader>(pageOffset(page_id));
}

const PageHeader* PageManager::pageHeader(uint16_t page_id) const {
    return file_.at<PageHeader>(pageOffset(page_id));
}

void PageManager::initPage(uint16_t page_id) {
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

void PageManager::growPages(uint16_t count) {
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

PageManager::PageManager(const std::string& db_dir)
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

uint16_t PageManager::allocPage() {
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

void PageManager::freePage(uint16_t page_id) {
    auto* hdr = fileHeader();

    // Reset page
    initPage(page_id);

    // Push vào đầu free list
    PageHeader* ph = pageHeader(page_id);
    ph->next_free_page = hdr->free_page_head;
    hdr->free_page_head = page_id;
}

// --- Slot-level operations ---

uint16_t PageManager::allocSlot(uint16_t page_id) {
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

void PageManager::freeSlot(uint16_t page_id, uint16_t slot_id) {
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

bool PageManager::hasAvailableSlot(uint16_t page_id) const {
    const PageHeader* ph = pageHeader(page_id);
    // Có slot trong free list, HOẶC chưa dùng hết capacity
    return (ph->free_slot_head != NULL_SLOT) ||
           (ph->slot_count < ph->slot_capacity);
}

// --- Edge access ---

DiskEdge* PageManager::getEdge(uint16_t page_id, uint16_t slot_id) {
    return file_.at<DiskEdge>(slotOffset(page_id, slot_id));
}

const DiskEdge* PageManager::getEdge(uint16_t page_id, uint16_t slot_id) const {
    return file_.at<DiskEdge>(slotOffset(page_id, slot_id));
}

// --- Metadata ---

uint32_t PageManager::pageCount() const {
    return fileHeader()->page_count;
}

// --- Linked List Operations ---

void PageManager::readEdgeChain(uint32_t start_page, uint16_t start_slot, utils::MiniVector<GenericEdge>& out_edges) const {
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

void PageManager::prependEdge(uint32_t& chain_page, uint16_t& chain_slot, const GenericEdge& edge) {
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

void PageManager::sync() {
    file_.sync();
}

} // namespace internal
} // namespace graphlite
