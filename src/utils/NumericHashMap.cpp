#include "../../include/graphlite/internal/NumericHashMap.h"
#include <cstdint>
#include <cstddef>

using namespace graphlite::utils;


size_t NumericHashMap::hash(uint64_t key) const {
    key = (~key) + (key << 21);
    key = key ^ (key >> 24);
    key = (key + (key << 3)) + (key << 8);
    key = key ^ (key >> 14);
    key = (key + (key << 2)) + (key << 4);
    key = key ^ (key >> 28);
    key = key + (key << 31);
    return static_cast<size_t>(key);
}

void NumericHashMap::rehash(size_t new_capacity) {
    Entry* old_table = table_;
    size_t old_capacity = capacity_;

    table_ = new Entry[new_capacity];
    for (size_t i = 0; i < new_capacity; ++i) {
        table_[i].is_occupied = false;
        table_[i].is_deleted = false;
    }
    capacity_ = new_capacity;
    size_ = 0;

    for (size_t i = 0; i < old_capacity; ++i) {
        if (old_table[i].is_occupied && !old_table[i].is_deleted) {
            put(old_table[i].key, old_table[i].value);
        }
    }
    delete[] old_table;
}

NumericHashMap::NumericHashMap(size_t initial_capacity) 
    : capacity_(initial_capacity), size_(0) {
    table_ = new Entry[capacity_];
    for (size_t i = 0; i < capacity_; ++i) {
        table_[i].is_occupied = false;
        table_[i].is_deleted = false;
    }
}

NumericHashMap::~NumericHashMap() { delete[] table_; }

void NumericHashMap::put(uint64_t key, size_t value) {
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

bool NumericHashMap::get(uint64_t key, size_t& out_value) const {
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

// Hỗ trợ xóa (cho quá trình Compaction sau này)
bool NumericHashMap::remove(uint64_t key) {
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
