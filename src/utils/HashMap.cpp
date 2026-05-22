#include <string>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include "HashMap.h"

size_t graphlite::utils::HashMap::hash(const std::string& key) const {
    size_t hash_val = 5381;
    for (char c : key) {
        hash_val = ((hash_val << 5) + hash_val) + c; // hash_val * 33 + c
    }
    return hash_val;
}

void graphlite::utils::HashMap::rehash(size_t new_capacity) {
    Entry* old_table = table_;
    size_t old_capacity = capacity_;

    table_ = new Entry[new_capacity];
    capacity_ = new_capacity;
    size_ = 0; // Sẽ đếm lại từ đầu khi insert

    for (size_t i = 0; i < old_capacity; ++i) {
        if (old_table[i].is_occupied && !old_table[i].is_deleted) {
            put(old_table[i].key, old_table[i].value);
        }
    }
    delete[] old_table;
}

// Khởi tạo sức chứa mặc định
graphlite::utils::HashMap::HashMap(size_t initial_capacity) 
    : capacity_(initial_capacity), size_(0) {
    table_ = new Entry[capacity_];
}

graphlite::utils::HashMap::~HashMap() {
    delete[] table_;
}

void graphlite::utils::HashMap::put(const std::string& key, size_t value) {
    // Load factor 70% thì mở rộng mảng để tránh đụng độ
    if (size_ * 10 >= capacity_ * 7) {
        rehash(capacity_ * 2);
    }

    size_t index = hash(key) % capacity_;
    
    // Linear Probing: Nếu slot đã có chủ (và không phải key của mình), đi tìm slot tiếp theo
    while (table_[index].is_occupied && !table_[index].is_deleted) {
        if (table_[index].key == key) {
            // Key đã tồn tại -> Ghi đè Value và return
            table_[index].value = value;
            return;
        }
        index = (index + 1) % capacity_; // Tiến lên 1 bước, xoay vòng nếu đụng rào
    }

    // Tìm được slot trống hoặc slot đã bị xóa
    table_[index].key = key;
    table_[index].value = value;
    table_[index].is_occupied = true;
    table_[index].is_deleted = false;
    size_++;
}

bool graphlite::utils::HashMap::get(const std::string& key, size_t& out_value) const {
    size_t index = hash(key) % capacity_;
    size_t start_index = index;

    while (table_[index].is_occupied) {
        if (!table_[index].is_deleted && table_[index].key == key) {
            out_value = table_[index].value;
            return true; // Tìm thấy!
        }
        index = (index + 1) % capacity_;
        if (index == start_index) break; // Duyệt hết 1 vòng mảng rồi (Map đầy)
    }
    return false; // Không tìm thấy
}

    // Cần hàm này cho việc duyệt Map lúc dọn rác (Compaction)
size_t graphlite::utils::HashMap::capacity() const { return capacity_; }
const graphlite::utils::HashMap::Entry& graphlite::utils::HashMap::getEntryAt(size_t index) const { return table_[index]; }


bool graphlite::utils::HashMap::remove(const std::string& key) {
    size_t index = hash(key) % capacity_;
    size_t start_index = index;

    while (table_[index].is_occupied) {
        if (!table_[index].is_deleted && table_[index].key == key) {
            // Đánh dấu xóa (Tombstone trên RAM), không gỡ hoàn toàn để 
            // không làm đứt chuỗi Linear Probing
            table_[index].is_deleted = true; 
            size_--;
            return true;
        }
        index = (index + 1) % capacity_;
        if (index == start_index) break;
    }
    return false;
}