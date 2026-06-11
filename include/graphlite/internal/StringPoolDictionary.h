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

#include "../types.h"  // Cho GRAPHLITE_FUNC, GRAPHLITE_IMPL_GUARD

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

#ifdef GRAPHLITE_IMPL_GUARD

GRAPHLITE_FUNC uint32_t StringPoolDictionary::hash_string(const char* str) const {
    // FNV-1a hash
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= static_cast<uint8_t>(*str++);
        hash *= 16777619u;
    }
    return hash;
}

GRAPHLITE_FUNC void StringPoolDictionary::resize_pool(uint32_t new_capacity) {
    char* new_pool = new char[new_capacity];
    if (pool_size_ > 0) {
        std::memcpy(new_pool, pool_, pool_size_);
    }
    delete[] pool_;
    pool_ = new_pool;
    pool_capacity_ = new_capacity;
}

GRAPHLITE_FUNC void StringPoolDictionary::rehash_table(uint32_t new_capacity) {
    DictEntry* old_table = table_;
    uint32_t old_capacity = table_capacity_;

    table_ = new DictEntry[new_capacity];
    for (uint32_t i = 0; i < new_capacity; ++i) {
        table_[i].is_occupied = false;
    }
    table_capacity_ = new_capacity;
    table_size_ = 0;

    for (uint32_t i = 0; i < old_capacity; ++i) {
        if (old_table[i].is_occupied) {
            const char* str = pool_ + old_table[i].pool_offset;
            insert_internal(str, old_table[i].pool_offset, old_table[i].id);
        }
    }
    delete[] old_table;
}

GRAPHLITE_FUNC void StringPoolDictionary::grow_reverse_lookup(uint32_t new_capacity) {
    uint32_t* new_arr = new uint32_t[new_capacity];
    std::memset(new_arr, 0, sizeof(uint32_t) * new_capacity);
    if (id_to_offset_ && id_to_offset_capacity_ > 0) {
        std::memcpy(new_arr, id_to_offset_, sizeof(uint32_t) * id_to_offset_capacity_);
    }
    delete[] id_to_offset_;
    id_to_offset_ = new_arr;
    id_to_offset_capacity_ = new_capacity;
}

GRAPHLITE_FUNC void StringPoolDictionary::insert_internal(
    const char* str, uint32_t offset, uint32_t id) {
    uint32_t index = hash_string(str) % table_capacity_;
    while (table_[index].is_occupied) {
        index = (index + 1) % table_capacity_;
    }
    table_[index].pool_offset = offset;
    table_[index].id = id;
    table_[index].is_occupied = true;
    table_size_++;
}

GRAPHLITE_FUNC StringPoolDictionary::StringPoolDictionary(
    uint32_t initial_pool_mb, uint32_t initial_table_slots) {
    
    pool_capacity_ = initial_pool_mb * 1024 * 1024;
    pool_size_ = 0;
    pool_ = new char[pool_capacity_];

    table_capacity_ = initial_table_slots;
    table_size_ = 0;
    table_ = new DictEntry[table_capacity_];
    for (uint32_t i = 0; i < table_capacity_; ++i) {
        table_[i].is_occupied = false;
    }

    // Reverse lookup array — khởi tạo với 1024 slots
    id_to_offset_capacity_ = 1024;
    id_to_offset_ = new uint32_t[id_to_offset_capacity_];
    std::memset(id_to_offset_, 0, sizeof(uint32_t) * id_to_offset_capacity_);

    next_id_ = 1;  // ID bắt đầu từ 1
}

GRAPHLITE_FUNC StringPoolDictionary::~StringPoolDictionary() {
    delete[] pool_;
    delete[] table_;
    delete[] id_to_offset_;
}

GRAPHLITE_FUNC uint32_t StringPoolDictionary::get_or_create_id(const std::string& str) {
    // Rehash nếu load factor > 70%
    if (table_size_ * 10 >= table_capacity_ * 7) {
        rehash_table(table_capacity_ * 2);
    }

    const char* c_str = str.c_str();
    uint32_t index = hash_string(c_str) % table_capacity_;
    uint32_t start_index = index;

    // Tìm kiếm trong hash table
    while (table_[index].is_occupied) {
        if (std::strcmp(pool_ + table_[index].pool_offset, c_str) == 0) {
            return table_[index].id;  // Đã tồn tại
        }
        index = (index + 1) % table_capacity_;
        if (index == start_index) break;
    }

    // Chưa tồn tại — tạo mới
    uint32_t str_len = static_cast<uint32_t>(str.length());

    // Grow pool nếu cần
    if (pool_size_ + str_len + 1 > pool_capacity_) {
        resize_pool(pool_capacity_ * 2);
    }

    // Copy chuỗi vào pool
    uint32_t current_offset = pool_size_;
    std::memcpy(pool_ + pool_size_, c_str, str_len + 1);
    pool_size_ += (str_len + 1);

    // Gán ID mới
    uint32_t new_id = next_id_++;

    // Grow reverse lookup nếu cần
    if (new_id >= id_to_offset_capacity_) {
        grow_reverse_lookup(id_to_offset_capacity_ * 2);
    }

    // Cập nhật reverse lookup
    id_to_offset_[new_id] = current_offset;

    // Insert vào hash table
    table_[index].pool_offset = current_offset;
    table_[index].id = new_id;
    table_[index].is_occupied = true;
    table_size_++;

    return new_id;
}

GRAPHLITE_FUNC uint32_t StringPoolDictionary::get_id(const std::string& str) const {
    const char* c_str = str.c_str();
    uint32_t index = hash_string(c_str) % table_capacity_;
    uint32_t start_index = index;

    while (table_[index].is_occupied) {
        if (std::strcmp(pool_ + table_[index].pool_offset, c_str) == 0) {
            return table_[index].id;
        }
        index = (index + 1) % table_capacity_;
        if (index == start_index) break;
    }
    return 0;  // Not found
}

GRAPHLITE_FUNC std::string StringPoolDictionary::get_string(uint32_t id) const {
    // v1.0: O(1) reverse lookup (thay vì O(n) linear scan của v0.1)
    if (id == 0 || id >= next_id_) {
        return "";
    }
    if (id >= id_to_offset_capacity_) {
        return "";
    }
    return std::string(pool_ + id_to_offset_[id]);
}

#endif // GRAPHLITE_IMPL_GUARD

} // namespace utils
} // namespace graphlite
