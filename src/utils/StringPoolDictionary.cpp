#include "StringPoolDictionary.h"
#include <cstring>

using namespace graphlite::utils;

uint32_t StringPoolDictionary::hash_string(const char* str) const {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= static_cast<uint8_t>(*str++);
        hash *= 16777619u;
    }
    return hash;
}

void StringPoolDictionary::resize_pool(uint32_t new_capacity) {
    char* new_pool = new char[new_capacity];
    if (pool_size_ > 0) {
        std::memcpy(new_pool, pool_, pool_size_);
    }
    delete[] pool_;
    pool_ = new_pool;
    pool_capacity_ = new_capacity;
}

void StringPoolDictionary::rehash_table(uint32_t new_capacity) {
    DictEntry* old_table = table_;
    uint32_t old_capacity = table_capacity_;

    table_ = new DictEntry[new_capacity];
    for (uint32_t i = 0; i < new_capacity; ++i) table_[i].is_occupied = false;
    
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

void StringPoolDictionary::insert_internal(const char* str, uint32_t offset, uint32_t id) {
    uint32_t index = hash_string(str) % table_capacity_;
    while (table_[index].is_occupied) {
        index = (index + 1) % table_capacity_;
    }
    table_[index].pool_offset = offset;
    table_[index].id = id;
    table_[index].is_occupied = true;
    table_size_++;
}

StringPoolDictionary::StringPoolDictionary(uint32_t initial_pool_mb, uint32_t initial_table_slots) {
    pool_capacity_ = initial_pool_mb * 1024 * 1024;
    pool_size_ = 0;
    pool_ = new char[pool_capacity_];

    table_capacity_ = initial_table_slots;
    table_size_ = 0;
    table_ = new DictEntry[table_capacity_];
    for (uint32_t i = 0; i < table_capacity_; ++i) table_[i].is_occupied = false;

    next_id_ = 1;
}

StringPoolDictionary::~StringPoolDictionary() {
    delete[] pool_;
    delete[] table_;
}

uint32_t StringPoolDictionary::get_or_create_id(const std::string& str) {
    if (table_size_ * 10 >= table_capacity_ * 7) {
        rehash_table(table_capacity_ * 2);
    }

    const char* c_str = str.c_str();
    uint32_t index = hash_string(c_str) % table_capacity_;
    uint32_t start_index = index;

    while (table_[index].is_occupied) {
        // Zero-allocation comparison
        if (std::strcmp(pool_ + table_[index].pool_offset, c_str) == 0) {
            return table_[index].id;
        }
        index = (index + 1) % table_capacity_;
        if (index == start_index) break;
    }

    uint32_t str_len = str.length();
    if (pool_size_ + str_len + 1 > pool_capacity_) {
        resize_pool(pool_capacity_ * 2);
    }

    uint32_t current_offset = pool_size_;
    std::memcpy(pool_ + pool_size_, c_str, str_len + 1);
    pool_size_ += (str_len + 1);

    uint32_t new_id = next_id_++;
    table_[index].pool_offset = current_offset;
    table_[index].id = new_id;
    table_[index].is_occupied = true;
    table_size_++;

    return new_id;
}


// Return 0 if not found, otherwise return the ID.
uint32_t StringPoolDictionary::get_id(const std::string& str) const {
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
    return 0; // Not found
}