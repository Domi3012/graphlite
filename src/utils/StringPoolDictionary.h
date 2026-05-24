#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

namespace graphlite {
namespace utils {

struct DictEntry {
    uint32_t pool_offset;
    uint32_t id;
    bool is_occupied;
};

class StringPoolDictionary {
private:
    char* pool_;
    uint32_t pool_capacity_;
    uint32_t pool_size_;

    DictEntry* table_;
    uint32_t table_capacity_;
    uint32_t table_size_;

    uint32_t next_id_;

    uint32_t hash_string(const char* str) const;
    void resize_pool(uint32_t new_capacity);
    void rehash_table(uint32_t new_capacity);
    void insert_internal(const char* str, uint32_t offset, uint32_t id);

public:
    explicit StringPoolDictionary(uint32_t initial_pool_mb = 50, uint32_t initial_table_slots = 1000000);
    ~StringPoolDictionary();

    StringPoolDictionary(const StringPoolDictionary&) = delete;
    StringPoolDictionary& operator=(const StringPoolDictionary&) = delete;

    uint32_t get_or_create_id(const std::string& str);
};

} // namespace utils
} // namespace graphlite