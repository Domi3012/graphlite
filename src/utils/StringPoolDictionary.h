/**
 * @file StringPoolDictionary.h
 * @brief Arena-based string dictionary — ánh xạ string ↔ uint32_t ID.
 * @version 2.0
 *
 * v2.0 Changes:
 * - Thêm mmap-backed mode: constructor nhận file_path → persistent trên đĩa
 * - Giữ backward-compatible heap-backed constructor cho Schema (nhẹ, không cần persist)
 * - Tự detect first-open vs reopen qua magic bytes
 *
 * Kiến trúc nội bộ:
 * - pool_: Vùng nhớ liên tục chứa mọi chuỗi (null-terminated, nối tiếp nhau)
 * - table_: Hash table (open addressing, linear probing) chứa DictEntry
 * - id_to_offset_: Mảng reverse lookup — id_to_offset_[id] = pool offset
 *
 * Mmap file layout (strings.gldb):
 * ┌──────────────────────────────────────────────────┐
 * │ StringPoolHeader (64B)                            │
 * │   magic[4], version, pool_size, next_id,          │
 * │   table_size, table_capacity, id_to_offset_cap    │
 * ├──────────────────────────────────────────────────┤
 * │ Pool region (pool_capacity bytes)                 │
 * ├──────────────────────────────────────────────────┤
 * │ Table region (table_capacity × sizeof(DictEntry)) │
 * ├──────────────────────────────────────────────────┤
 * │ Reverse lookup (id_to_offset_cap × sizeof(u32))   │
 * └──────────────────────────────────────────────────┘
 */

#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <stdexcept>

#include <graphlite/types.h>

namespace graphlite {
namespace utils {

/** @brief Entry trong hash table. */
struct DictEntry {
    uint32_t pool_offset;  ///< Vị trí chuỗi trong pool.
    uint32_t id;           ///< ID ánh xạ.
    bool     is_occupied;  ///< Slot có dữ liệu?
};

/**
 * @struct StringPoolHeader
 * @brief Header ở đầu file strings.gldb — CHÍNH XÁC 64 bytes.
 */
struct StringPoolHeader {
    uint8_t  magic[4];              ///< 4 bytes — {'G','L','S','D'}
    uint32_t version;               ///< 4 bytes — format version (= 1)
    uint32_t pool_size;             ///< 4 bytes — bytes đã dùng trong pool
    uint32_t pool_capacity;         ///< 4 bytes — tổng bytes pool
    uint32_t next_id;               ///< 4 bytes — ID kế tiếp
    uint32_t table_size;            ///< 4 bytes — số entries occupied
    uint32_t table_capacity;        ///< 4 bytes — tổng slots
    uint32_t id_to_offset_capacity; ///< 4 bytes — capacity reverse lookup
    uint8_t  _reserved[32];         ///< 32 bytes — dự trữ
};
static_assert(sizeof(StringPoolHeader) == 64, "StringPoolHeader must be 64 bytes");

/**
 * @class StringPoolDictionary
 * @brief Từ điển string → uint32_t với Arena allocator.
 *
 * Đảm bảo mỗi chuỗi có đúng 1 ID duy nhất (intern pattern).
 * IDs bắt đầu từ 1 (0 = not found).
 *
 * Hai chế độ:
 * 1. Heap mode: StringPoolDictionary(pool_mb, table_slots) — tạm thời, trong RAM
 * 2. Mmap mode: StringPoolDictionary(file_path) — persistent, trên đĩa
 */
class StringPoolDictionary {
private:
    // --- String Pool (Arena) ---
    char*    pool_;
    uint32_t pool_capacity_;
    uint32_t pool_size_;

    // --- Hash Table ---
    DictEntry* table_;
    uint32_t   table_capacity_;
    uint32_t   table_size_;

    // --- Reverse Lookup: ID → pool offset ---
    uint32_t*  id_to_offset_;
    uint32_t   id_to_offset_capacity_;

    // --- Auto-increment ID ---
    uint32_t next_id_;

    // --- Mmap state ---
    bool is_mmap_mode_;          ///< true nếu đang dùng mmap-backed
    void* mmap_base_;            ///< Base pointer toàn bộ mmap region
    size_t mmap_total_size_;     ///< Tổng kích thước mmap file
    int    mmap_fd_;             ///< File descriptor (POSIX) hoặc HANDLE (Windows)
    std::string mmap_file_path_; ///< Đường dẫn file (dùng cho grow + remap)

    // --- Internal methods ---
    uint32_t hash_string(const char* str) const;
    void resize_pool(uint32_t new_capacity);
    void rehash_table(uint32_t new_capacity);
    void grow_reverse_lookup(uint32_t new_capacity);
    void insert_internal(const char* str, uint32_t offset, uint32_t id);

    // --- Mmap helpers ---
    void mmap_remap();           ///< Remap toàn bộ file khi cần grow
    void mmap_grow_file(size_t new_size); ///< Grow file + remap
    size_t calc_mmap_size(uint32_t pool_cap, uint32_t table_cap, uint32_t id_cap) const;
    void update_pointers_from_mmap(); ///< Tính lại pool_, table_, id_to_offset_ từ mmap_base_
    void flush_header();         ///< Ghi metadata ngược lại header

    static constexpr uint8_t  MAGIC[4] = {'G', 'L', 'S', 'D'};
    static constexpr uint32_t FORMAT_VERSION = 1;

public:
    /**
     * @brief Constructor heap-backed (tạm thời, trong RAM).
     * @param initial_pool_mb Kích thước pool ban đầu (MB). Default: 4MB.
     * @param initial_table_slots Số slot hash table. Default: 65536.
     */
    explicit StringPoolDictionary(uint32_t initial_pool_mb = 4, 
                                  uint32_t initial_table_slots = 65536);

    /**
     * @brief Constructor mmap-backed (persistent, trên đĩa).
     * @param file_path Đường dẫn file strings.gldb.
     *
     * Nếu file mới → khởi tạo header + pool + table.
     * Nếu file cũ → đọc header, verify magic, khôi phục state.
     */
    explicit StringPoolDictionary(const std::string& file_path);

    ~StringPoolDictionary();

    // Chặn copy
    StringPoolDictionary(const StringPoolDictionary&) = delete;
    StringPoolDictionary& operator=(const StringPoolDictionary&) = delete;

    /**
     * @brief Lấy ID của chuỗi. Tạo mới nếu chưa tồn tại (upsert).
     * @param str Chuỗi cần tra cứu/tạo.
     * @return uint32_t ID (>= 1). Chuỗi giống nhau luôn trả cùng ID.
     */
    uint32_t get_or_create_id(const std::string& str);

    /**
     * @brief Tra cứu ID (không tạo mới nếu không tồn tại).
     * @param str Chuỗi cần tra cứu.
     * @return uint32_t ID nếu tồn tại, 0 nếu không.
     */
    uint32_t get_id(const std::string& str) const;

    /**
     * @brief Lấy số lượng ID đã được cấp phát.
     * @return Số lượng ID.
     */
    uint32_t get_id_count() const;

    /**
     * @brief Tra cứu ngược: ID → string — O(1).
     * @param id ID cần tra cứu.
     * @return Chuỗi tương ứng, hoặc "" nếu ID không tồn tại.
     */
    std::string get_string(uint32_t id) const;

    /**
     * @brief Flush dữ liệu xuống đĩa (chỉ có hiệu lực ở mmap mode).
     */
    void sync();

    /** @brief Kiểm tra đang dùng mmap mode hay heap mode. */
    bool isMmapMode() const { return is_mmap_mode_; }
};

} // namespace utils
} // namespace graphlite
