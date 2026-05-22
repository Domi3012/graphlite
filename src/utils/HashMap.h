#pragma once
#include <string>
#include <cstdint>

namespace graphlite {
namespace utils {

// Linear Probing
// Algorithm: DJB2, hash string -> size_t
class HashMap {
private:
    struct Entry {
        std::string key;
        size_t value;
        bool is_occupied = false;
        bool is_deleted = false;
    };

    Entry* table_;
    size_t capacity_;
    size_t size_;

    // Thuật toán băm DJB2 siêu tốc
    size_t hash(const std::string& key) const;

    // Nhân đôi mảng khi Load Factor > 0.7
    void rehash(size_t new_capacity);

public:
    // Khởi tạo sức chứa mặc định
    explicit HashMap(size_t initial_capacity = 128);
    
    ~HashMap();

    HashMap(const HashMap&) = delete;
    HashMap& operator=(const HashMap&) = delete;

    void put(const std::string& key, size_t value);

    bool get(const std::string& key, size_t& out_value) const;

    bool remove(const std::string& key);

    size_t capacity() const;
    const Entry& getEntryAt(size_t index) const;
};

} // namespace utils
} // namespace graphlite