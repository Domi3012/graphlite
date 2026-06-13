/**
 * @file types.h
 * @brief Định nghĩa kiểu dữ liệu cốt lõi, cấu trúc Edge/Node, và macro cấu hình thư viện.
 * @version 1.0
 * @date 2026-06
 *
 * File này là nền tảng của toàn bộ GraphLite:
 * - Macro cấu hình library mode (compiled vs header-only)
 * - Kiểu định danh hệ thống (NodeID, EdgeType, PageID, SlotID)
 * - Cấu trúc vật lý GenericEdge (RAM), DiskEdge (đĩa), NodeRecord (đĩa)
 *
 * Tất cả struct POD được thiết kế với natural alignment để tránh padding.
 */

#pragma once



// ============================================================
// SYSTEM INCLUDES
// ============================================================
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace graphlite {

// ============================================================
// ĐỊNH DANH HỆ THỐNG (SYSTEM IDENTIFIERS)
// ============================================================

/**
 * @typedef NodeID
 * @brief Định danh nguyên thủy của Đỉnh.
 * Sử dụng số nguyên không dấu 32-bit, cung cấp không gian cho ~4.2 tỷ đỉnh.
 */
using NodeID = uint32_t;

/**
 * @typedef NodeType
 * @brief Định danh phân loại Đỉnh (Ví dụ: User, Device).
 * 8-bit, giới hạn tối đa 255 loại.
 */
using NodeType = uint8_t;

/**
 * @typedef EdgeType
 * @brief Định danh phân loại Cạnh (Ví dụ: USES, CONNECTS).
 * 8-bit, giới hạn tối đa 255 loại.
 */
using EdgeType = uint8_t;

/**
 * @typedef PageID
 * @brief Định danh trang trong edge file (edges.gldb).
 */
using PageID = uint32_t;

/**
 * @typedef SlotID
 * @brief Định danh slot bên trong một page.
 */
using SlotID = uint16_t;

/** @brief Giá trị sentinel cho PageID — biểu thị "không có page". */
constexpr PageID NULL_PAGE = 0xFFFFFFFF;

/** @brief Giá trị sentinel cho SlotID — biểu thị "không có slot". */
constexpr SlotID NULL_SLOT = 0xFFFF;

// ============================================================
// CẤU TRÚC VẬT LÝ CỦA MẠNG LƯỚI (GRAPH STRUCTURES)
// ============================================================

/**
 * @brief Kích thước payload tối đa cho mỗi edge.
 * @note 11 bytes = 32B(DiskEdge) - 4B(src) - 4B(tgt) - 1B(type) - 6B(next) - 6B(next_in)
 * GenericEdge (RAM view) cũng dùng 11B payload để tương thích 1:1 với DiskEdge.
 */
constexpr uint8_t MAX_PAYLOAD_SIZE = 11;

/**
 * @struct GenericEdge
 * @brief Cấu trúc biểu diễn một Cạnh trong RAM (API layer).
 *
 * GenericEdge không trực tiếp định nghĩa thuộc tính (timestamp, weight).
 * Thay vào đó, nó mang một mảng byte vô danh (payload). Tầng Application
 * sẽ tự ép kiểu (reinterpret_cast) mảng byte này thành struct nghiệp vụ.
 *
 * @note sizeof(GenericEdge) = 28 bytes (4 + 1 + 23, no padding).
 */
struct GenericEdge {
    NodeID   source_node;                    ///< ID của đỉnh nguồn.
    NodeID   target_node;                    ///< ID của đỉnh đích mà cạnh này trỏ tới.
    EdgeType edge_type;                      ///< Mã phân loại mối quan hệ.
    uint8_t  payload[MAX_PAYLOAD_SIZE];      ///< Vùng nhớ đệm chứa dữ liệu tùy chỉnh.

    /** @brief Constructor mặc định. */
    GenericEdge() = default;

    /**
     * @brief Constructor khởi tạo Cạnh an toàn.
     * @param source ID của đỉnh nguồn.
     * @param target ID của đỉnh đích.
     * @param type Loại cạnh.
     * @param raw_payload Con trỏ trỏ tới struct dữ liệu (Ví dụ: &my_struct).
     * @param payload_size Kích thước thực tế của struct (dùng sizeof).
     */
    GenericEdge(NodeID source, NodeID target, EdgeType type, const uint8_t* raw_payload, uint8_t payload_size)
        : source_node(source), target_node(target), edge_type(type) {
        std::memset(payload, 0, MAX_PAYLOAD_SIZE);
        if (raw_payload != nullptr && payload_size > 0) {
            uint8_t copy_size = std::min(payload_size, MAX_PAYLOAD_SIZE);
            std::memcpy(payload, raw_payload, copy_size);
        }
    }
};

/**
 * @struct DiskEdge
 * @brief Layout vật lý của Cạnh trên ổ đĩa — CHÍNH XÁC 32 bytes.
 *
 * 32 bytes = lũy thừa 2 → chính xác 2 edges trên 1 cache line (64B).
 * Pointer arithmetic dùng bit-shift (<< 5) thay vì phép nhân.
 *
 * Chứa linked-list pointers (next_page, next_slot) cho index-free adjacency.
 */
struct DiskEdge {
    NodeID   source_node;                    ///< 4 bytes — ID đỉnh nguồn.
    NodeID   target_node;                    ///< 4 bytes — ID đỉnh đích.
    uint32_t next_page;                      ///< 4 bytes — Page chứa out-edge kế tiếp
    uint32_t next_in_page;                   ///< 4 bytes — Page chứa in-edge kế tiếp
    uint16_t next_slot;                      ///< 2 bytes — Slot chứa out-edge kế tiếp
    uint16_t next_in_slot;                   ///< 2 bytes — Slot chứa in-edge kế tiếp
    EdgeType edge_type;                      ///< 1 byte  — Loại cạnh.
    uint8_t  payload[MAX_PAYLOAD_SIZE];      ///< 11 bytes — Payload vô danh.
};
static_assert(sizeof(DiskEdge) == 32, "DiskEdge must be exactly 32 bytes");

/**
 * @struct NodeRecord
 * @brief Metadata của một Đỉnh trên ổ đĩa — CHÍNH XÁC 32 bytes.
 *
 * Được sắp xếp theo natural alignment (uint32_t trước, uint16_t sau, uint8_t cuối)
 * để tránh compiler padding.
 *
 * Nằm trong flat array trên mmap: record = &array[node_id] → O(1).
 */
struct NodeRecord {
    uint32_t first_edge_page;                ///< 4 bytes — Page chứa out-edge đầu tiên.
    uint32_t edge_count;                     ///< 4 bytes — Tổng số out-edges.
    uint16_t first_edge_slot;                ///< 2 bytes — Slot chứa out-edge đầu tiên.
    NodeType node_type;                      ///< 1 byte  — Phân loại đỉnh.
    uint8_t  flags;                          ///< 1 byte  — Cờ trạng thái (0=empty, 1=active).
    
    // Bidirectional & Payload support (20 bytes)
    uint32_t first_in_edge_page;             ///< 4 bytes — Page chứa in-edge đầu tiên.
    uint32_t in_edge_count;                  ///< 4 bytes — Tổng số in-edges.
    uint16_t first_in_edge_slot;             ///< 2 bytes — Slot chứa in-edge đầu tiên.
    uint8_t  payload[8];                     ///< 8 bytes — Payload tùy chỉnh của Đỉnh.
    uint8_t  _reserved[2];                   ///< 2 bytes — Dự trữ.
};
static_assert(sizeof(NodeRecord) == 32, "NodeRecord must be exactly 32 bytes");

// ============================================================
// FILE HEADERS — Metadata ở đầu mỗi database file (64 bytes each)
// ============================================================

/** @brief Kích thước trang vật lý (trùng OS page size). */
constexpr uint32_t GLDB_PAGE_SIZE = 4096;

/** @brief Số DiskEdge tối đa trên 1 page: (4096 - 64) / 32 = 126. */
constexpr uint16_t EDGES_PER_PAGE = (GLDB_PAGE_SIZE - 64) / sizeof(DiskEdge);

/**
 * @struct NodeFileHeader
 * @brief Header ở đầu file `nodes.gldb` — CHÍNH XÁC 64 bytes.
 *
 * Layout:
 * ```
 * Byte: 0─3    4─7       8─11         12─15    16─63
 *       magic  version   node_count   next_id  _reserved
 * ```
 */
struct NodeFileHeader {
    uint8_t  magic[4];           ///< 4 bytes — Nhận dạng file: {'G','L','N','D'}
    uint32_t version;            ///< 4 bytes — Phiên bản format (= 1)
    uint32_t node_count;         ///< 4 bytes — Số đỉnh đang active
    uint32_t next_id;            ///< 4 bytes — ID kế tiếp sẽ cấp phát
    uint8_t  _reserved[48];      ///< 48 bytes — Dự trữ tương lai
};
static_assert(sizeof(NodeFileHeader) == 64, "NodeFileHeader must be exactly 64 bytes");

/**
 * @struct EdgeFileHeader
 * @brief Header ở đầu file `edges.gldb` — CHÍNH XÁC 64 bytes.
 *
 * Layout:
 * ```
 * Byte: 0─3    4─7       8─11        12─15        16─17          18─63
 *       magic  version   page_size   page_count   free_page_head _reserved
 * ```
 */
struct EdgeFileHeader {
    uint8_t  magic[4];           ///< 4 bytes — Nhận dạng file: {'G','L','E','D'}
    uint32_t version;            ///< 4 bytes — Phiên bản format (= 1)
    uint32_t page_size;          ///< 4 bytes — Kích thước page (= 4096)
    uint32_t page_count;         ///< 4 bytes — Tổng số pages đã cấp phát
    uint16_t free_page_head;     ///< 2 bytes — Đầu free-list page (NULL_SLOT nếu hết)
    uint8_t  _reserved[46];      ///< 46 bytes — Dự trữ tương lai
};
static_assert(sizeof(EdgeFileHeader) == 64, "EdgeFileHeader must be exactly 64 bytes");

/**
 * @struct PageHeader
 * @brief Header ở đầu mỗi page trong `edges.gldb` — CHÍNH XÁC 64 bytes.
 *
 * Mỗi page 4096B = PageHeader(64B) + DiskEdge[126](4032B).
 *
 * Layout:
 * ```
 * Byte: 0─1          2─3             4─5             6─7              8─63
 *       slot_count   slot_capacity   free_slot_head  next_free_page   _reserved
 * ```
 */
struct PageHeader {
    uint16_t slot_count;         ///< 2 bytes — Số slot đang sử dụng
    uint16_t slot_capacity;      ///< 2 bytes — Sức chứa tối đa (= 126)
    uint16_t free_slot_head;     ///< 2 bytes — Đầu free-list slot (NULL_SLOT nếu hết)
    uint16_t next_free_page;     ///< 2 bytes — Page trống kế tiếp (cho file-level free list)
    uint8_t  _reserved[56];      ///< 56 bytes — Dự trữ tương lai
};
static_assert(sizeof(PageHeader) == 64, "PageHeader must be exactly 64 bytes");

} // namespace graphlite