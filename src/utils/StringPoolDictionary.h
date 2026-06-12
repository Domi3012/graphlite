/**
 * @file StringPoolDictionary.h
 * @brief Arena-based string dictionary — ánh xạ string ↔ uint32_t ID.
 * @version 1.0
 *
 * Cải tiến so với v0.1:
 * - get_string(id) từ O(n) → O(1) nhờ reverse-lookup array
 * - Default pool giảm từ 50MB → 4MB (tự grow)
 * - Default table slots giảm từ 1M → 65536 (tự rehash)
 * - Hybrid macro pattern (GRAPHLITE_IMPL_GUARD)
 *
 * Kiến trúc nội bộ:
 * - pool_: Vùng nhớ liên tục chứa mọi chuỗi (null-terminated, nối tiếp nhau)
 * - table_: Hash table (open addressing, linear probing) chứa DictEntry
 * - id_to_offset_: Mảng reverse lookup — id_to_offset_[id] = pool offset
 */

#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <stdexcept>

#include <graphlite/types.h>  // Cho GRAPHLITE_FUNC, GRAPHLITE_IMPL_GUARD

namespace graphlite {
namespace utils {

/** @brief Entry trong hash table. */
struct DictEntry {
    uint32_t pool_offset;  ///< Vị trí chuỗi trong pool.
    uint32_t id;           ///< ID ánh xạ.
    bool     is_occupied;  ///< Slot có dữ liệu?
};

/**
 * @class StringPoolDictionary
 * @brief Từ điển string → uint32_t với Arena allocator.
 *
 * Đảm bảo mỗi chuỗi có đúng 1 ID duy nhất (intern pattern).
 * IDs bắt đầu từ 1 (0 = not found).
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

    // --- Reverse Lookup: ID → pool offset (MỚI v1.0) ---
    uint32_t*  id_to_offset_;
    uint32_t   id_to_offset_capacity_;

    // --- Auto-increment ID ---
    uint32_t next_id_;

    // --- Internal methods ---
    uint32_t hash_string(const char* str) const;
    void resize_pool(uint32_t new_capacity);
    void rehash_table(uint32_t new_capacity);
    void grow_reverse_lookup(uint32_t new_capacity);
    void insert_internal(const char* str, uint32_t offset, uint32_t id);

public:
    /**
     * @brief Constructor.
     * @param initial_pool_mb Kích thước pool ban đầu (MB). Default: 4MB.
     * @param initial_table_slots Số slot hash table. Default: 65536.
     */
    explicit StringPoolDictionary(uint32_t initial_pool_mb = 4, 
                                  uint32_t initial_table_slots = 65536);
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
     * @brief Tra cứu ngược: ID → string.
     * @note v1.0: O(1) nhờ reverse-lookup array (v0.1 là O(n) scan).
     * @param id ID cần tra cứu.
     * @return Chuỗi tương ứng, hoặc "" nếu ID không tồn tại.
     */
    std::string get_string(uint32_t id) const;
};

// ============================================================
// IMPLEMENTATION
// ============================================================

} // namespace utils
} // namespace graphlite
