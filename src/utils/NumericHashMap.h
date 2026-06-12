/**
 * @file NumericHashMap.h
 * @brief Bảng băm chuyên dụng cho khóa uint64_t → giá trị size_t.
 * @version 1.0
 *
 * Sử dụng Open Addressing với Linear Probing.
 * Hàm băm: Thomas Wang's 64-bit integer hash — phân phối cực đều cho ID tuần tự.
 *
 * Dùng hybrid macro pattern (GRAPHLITE_IMPL_GUARD).
 */

#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>

#include <graphlite/types.h>  // Cho inline, GRAPHLITE_IMPL_GUARD

namespace graphlite {
namespace utils {

/**
 * @class NumericHashMap
 * @brief Bảng băm tối ưu: uint64_t (Key) → size_t (Value).
 *
 * Dùng cho:
 * - Bitcask KeyDir: NodeID → file offset  (v0.1, đã loại bỏ)
 * - CLOCK Cache: NodeID → ring index     (v1.0)
 * - Bất kỳ mapping integer → integer nào
 */
class NumericHashMap {
private:
    struct Entry {
        uint64_t key;
        size_t   value;
        bool     is_occupied;
        bool     is_deleted;
    };

    Entry*  table_;
    size_t  capacity_;
    size_t  size_;

    /** @brief Thomas Wang's 64-bit integer hash. */
    size_t hash(uint64_t key) const;

    /** @brief Rehash toàn bộ bảng sang capacity mới. */
    void rehash(size_t new_capacity);

public:
    explicit NumericHashMap(size_t initial_capacity = 16384);
    ~NumericHashMap();

    // Chặn copy
    NumericHashMap(const NumericHashMap&) = delete;
    NumericHashMap& operator=(const NumericHashMap&) = delete;

    /** @brief Thêm/cập nhật entry. Rehash nếu load factor > 70%. */
    void put(uint64_t key, size_t value);

    /** @brief Tìm value theo key. Trả về true nếu tìm thấy. */
    bool get(uint64_t key, size_t& out_value) const;

    /** @brief Xóa entry (lazy delete — đánh dấu is_deleted). */
    bool remove(uint64_t key);

    /** @brief Số entry đang sử dụng. */
    size_t size() const { return size_; }
};

// ============================================================
// IMPLEMENTATION
// ============================================================


inline size_t NumericHashMap::hash(uint64_t key) const {
    key = (~key) + (key << 21);
    key = key ^ (key >> 24);
    key = (key + (key << 3)) + (key << 8);
    key = key ^ (key >> 14);
    key = (key + (key << 2)) + (key << 4);
    key = key ^ (key >> 28);
    key = key + (key << 31);
    return static_cast<size_t>(key);
}

inline void NumericHashMap::rehash(size_t new_capacity) {
    Entry* old_table = table_;
    size_t old_capacity = capacity_;

    table_ = new Entry[new_capacity];
    std::memset(table_, 0, sizeof(Entry) * new_capacity);
    capacity_ = new_capacity;
    size_ = 0;

    for (size_t i = 0; i < old_capacity; ++i) {
        if (old_table[i].is_occupied && !old_table[i].is_deleted) {
            put(old_table[i].key, old_table[i].value);
        }
    }
    delete[] old_table;
}

inline NumericHashMap::NumericHashMap(size_t initial_capacity)
    : capacity_(initial_capacity), size_(0) {
    table_ = new Entry[capacity_];
    std::memset(table_, 0, sizeof(Entry) * capacity_);
}

inline NumericHashMap::~NumericHashMap() {
    delete[] table_;
}

inline void NumericHashMap::put(uint64_t key, size_t value) {
    if (size_ * 10 >= capacity_ * 7) rehash(capacity_ * 2);

    size_t index = hash(key) % capacity_;

    while (table_[index].is_occupied && !table_[index].is_deleted) {
        if (table_[index].key == key) {
            table_[index].value = value;
            return;
        }
        index = (index + 1) % capacity_;
    }

    table_[index].key = key;
    table_[index].value = value;
    table_[index].is_occupied = true;
    table_[index].is_deleted = false;
    size_++;
}

inline bool NumericHashMap::get(uint64_t key, size_t& out_value) const {
    size_t index = hash(key) % capacity_;
    size_t start_index = index;

    while (table_[index].is_occupied) {
        if (!table_[index].is_deleted && table_[index].key == key) {
            out_value = table_[index].value;
            return true;
        }
        index = (index + 1) % capacity_;
        if (index == start_index) break;
    }
    return false;
}

inline bool NumericHashMap::remove(uint64_t key) {
    size_t index = hash(key) % capacity_;
    size_t start_index = index;

    while (table_[index].is_occupied) {
        if (!table_[index].is_deleted && table_[index].key == key) {
            table_[index].is_deleted = true;
            size_--;
            return true;
        }
        index = (index + 1) % capacity_;
        if (index == start_index) break;
    }
    return false;
}


} // namespace utils
} // namespace graphlite
