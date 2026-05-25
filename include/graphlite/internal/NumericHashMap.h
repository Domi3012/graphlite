#pragma once
#include <cstdint>
#include <cstddef>

namespace graphlite {
namespace utils {

// Bảng băm chuyên dụng cho Bitcask KeyDir: uint64_t (Key) -> size_t (Offset)
class NumericHashMap {
private:
    struct Entry {
        uint64_t key;
        size_t value; // File Offset
        bool is_occupied;
        bool is_deleted;
    };

    Entry* table_;
    size_t capacity_;
    size_t size_;

    // Thuật toán Thomas Wang's 64-bit to 32-bit Hash
    // Cực kỳ hoàn hảo cho việc băm các ID hoặc composite key
    size_t hash(uint64_t key) const;

    void rehash(size_t new_capacity);

public:
    explicit NumericHashMap(size_t initial_capacity = 100000);

    ~NumericHashMap();
    NumericHashMap(const NumericHashMap&) = delete;
    NumericHashMap& operator=(const NumericHashMap&) = delete;

    void put(uint64_t key, size_t value);

    bool get(uint64_t key, size_t& out_value) const;
    
    // Hỗ trợ xóa (cho quá trình Compaction sau này)
    bool remove(uint64_t key);
};

} // namespace utils
} // namespace graphlite