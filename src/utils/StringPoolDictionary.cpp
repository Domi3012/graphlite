#include "StringPoolDictionary.h"
#include "../storage/Platform.h"
#include <cstring>
#include <stdexcept>

namespace graphlite {
namespace utils {

// ============================================================
// HASH FUNCTION (FNV-1a)
// ============================================================

uint32_t StringPoolDictionary::hash_string(const char* str) const {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= static_cast<uint8_t>(*str++);
        hash *= 16777619u;
    }
    return hash;
}

// ============================================================
// HEAP MODE — resize helpers
// ============================================================

void StringPoolDictionary::resize_pool(uint32_t new_capacity) {
    if (is_mmap_mode_) {
        // Mmap mode: grow file thay vì realloc
        uint32_t old_table_cap = table_capacity_;
        uint32_t old_id_cap = id_to_offset_capacity_;
        size_t new_size = calc_mmap_size(new_capacity, old_table_cap, old_id_cap);
        mmap_grow_file(new_size);
        pool_capacity_ = new_capacity;
        flush_header();
        return;
    }

    char* new_pool = new char[new_capacity];
    if (pool_size_ > 0) {
        std::memcpy(new_pool, pool_, pool_size_);
    }
    delete[] pool_;
    pool_ = new_pool;
    pool_capacity_ = new_capacity;
}

void StringPoolDictionary::rehash_table(uint32_t new_capacity) {
    if (is_mmap_mode_) {
        // Mmap mode: grow file, rebuild hash table in-place
        size_t new_size = calc_mmap_size(pool_capacity_, new_capacity, id_to_offset_capacity_);
        
        // Lưu tạm old table data trên heap
        uint32_t old_capacity = table_capacity_;
        DictEntry* old_table = new DictEntry[old_capacity];
        std::memcpy(old_table, table_, sizeof(DictEntry) * old_capacity);
        
        mmap_grow_file(new_size);
        table_capacity_ = new_capacity;
        
        // Clear new table
        for (uint32_t i = 0; i < table_capacity_; ++i) {
            table_[i].is_occupied = false;
        }
        table_size_ = 0;
        
        // Reinsert
        for (uint32_t i = 0; i < old_capacity; ++i) {
            if (old_table[i].is_occupied) {
                const char* str = pool_ + old_table[i].pool_offset;
                insert_internal(str, old_table[i].pool_offset, old_table[i].id);
            }
        }
        delete[] old_table;
        flush_header();
        return;
    }

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

void StringPoolDictionary::grow_reverse_lookup(uint32_t new_capacity) {
    if (is_mmap_mode_) {
        size_t new_size = calc_mmap_size(pool_capacity_, table_capacity_, new_capacity);
        
        // Lưu tạm old data
        uint32_t old_cap = id_to_offset_capacity_;
        uint32_t* old_data = new uint32_t[old_cap];
        std::memcpy(old_data, id_to_offset_, sizeof(uint32_t) * old_cap);
        
        mmap_grow_file(new_size);
        id_to_offset_capacity_ = new_capacity;
        
        // Copy old data back + zero-fill new area
        std::memcpy(id_to_offset_, old_data, sizeof(uint32_t) * old_cap);
        std::memset(id_to_offset_ + old_cap, 0, sizeof(uint32_t) * (new_capacity - old_cap));
        
        delete[] old_data;
        flush_header();
        return;
    }

    uint32_t* new_arr = new uint32_t[new_capacity];
    std::memset(new_arr, 0, sizeof(uint32_t) * new_capacity);
    if (id_to_offset_ && id_to_offset_capacity_ > 0) {
        std::memcpy(new_arr, id_to_offset_, sizeof(uint32_t) * id_to_offset_capacity_);
    }
    delete[] id_to_offset_;
    id_to_offset_ = new_arr;
    id_to_offset_capacity_ = new_capacity;
}

void StringPoolDictionary::insert_internal(
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

// ============================================================
// MMAP HELPERS
// ============================================================

size_t StringPoolDictionary::calc_mmap_size(uint32_t pool_cap, uint32_t table_cap, uint32_t id_cap) const {
    return sizeof(StringPoolHeader) +
           static_cast<size_t>(pool_cap) +
           static_cast<size_t>(table_cap) * sizeof(DictEntry) +
           static_cast<size_t>(id_cap) * sizeof(uint32_t);
}

void StringPoolDictionary::update_pointers_from_mmap() {
    auto* hdr = reinterpret_cast<StringPoolHeader*>(mmap_base_);
    uint8_t* base = reinterpret_cast<uint8_t*>(mmap_base_);

    pool_            = reinterpret_cast<char*>(base + sizeof(StringPoolHeader));
    pool_capacity_   = hdr->pool_capacity;
    pool_size_       = hdr->pool_size;

    table_           = reinterpret_cast<DictEntry*>(base + sizeof(StringPoolHeader) + hdr->pool_capacity);
    table_capacity_  = hdr->table_capacity;
    table_size_      = hdr->table_size;

    id_to_offset_    = reinterpret_cast<uint32_t*>(
        base + sizeof(StringPoolHeader) + hdr->pool_capacity +
        static_cast<size_t>(hdr->table_capacity) * sizeof(DictEntry));
    id_to_offset_capacity_ = hdr->id_to_offset_capacity;

    next_id_         = hdr->next_id;
}

void StringPoolDictionary::flush_header() {
    if (!is_mmap_mode_ || !mmap_base_) return;
    auto* hdr = reinterpret_cast<StringPoolHeader*>(mmap_base_);
    hdr->pool_size       = pool_size_;
    hdr->pool_capacity   = pool_capacity_;
    hdr->next_id         = next_id_;
    hdr->table_size      = table_size_;
    hdr->table_capacity  = table_capacity_;
    hdr->id_to_offset_capacity = id_to_offset_capacity_;
}

void StringPoolDictionary::mmap_grow_file(size_t new_size) {
    // Flush current header before remap
    flush_header();

    // Use Platform layer to grow + remap
    platform::MmapHandle handle;
    handle.data = mmap_base_;
    handle.length = mmap_total_size_;
    handle.fd   = mmap_fd_;

    platform::mmap_grow(handle, new_size);

    mmap_base_       = handle.data;
    mmap_total_size_ = handle.length;
    mmap_fd_         = handle.fd;

    // Recalculate pointers
    update_pointers_from_mmap();
}

void StringPoolDictionary::mmap_remap() {
    // Simply call grow with same size to force remap
    mmap_grow_file(mmap_total_size_);
}

// ============================================================
// CONSTRUCTORS
// ============================================================

// --- Heap mode (backward-compatible) ---
StringPoolDictionary::StringPoolDictionary(
    uint32_t initial_pool_mb, uint32_t initial_table_slots)
    : is_mmap_mode_(false), mmap_base_(nullptr), mmap_total_size_(0), mmap_fd_(-1) {
    
    pool_capacity_ = initial_pool_mb * 1024 * 1024;
    pool_size_ = 0;
    pool_ = new char[pool_capacity_];

    table_capacity_ = initial_table_slots;
    table_size_ = 0;
    table_ = new DictEntry[table_capacity_];
    for (uint32_t i = 0; i < table_capacity_; ++i) {
        table_[i].is_occupied = false;
    }

    id_to_offset_capacity_ = 1024;
    id_to_offset_ = new uint32_t[id_to_offset_capacity_];
    std::memset(id_to_offset_, 0, sizeof(uint32_t) * id_to_offset_capacity_);

    next_id_ = 1;
}

// --- Mmap mode (persistent) ---
StringPoolDictionary::StringPoolDictionary(const std::string& file_path)
    : pool_(nullptr), pool_capacity_(0), pool_size_(0),
      table_(nullptr), table_capacity_(0), table_size_(0),
      id_to_offset_(nullptr), id_to_offset_capacity_(0),
      next_id_(1),
      is_mmap_mode_(true), mmap_base_(nullptr), mmap_total_size_(0), mmap_fd_(-1),
      mmap_file_path_(file_path) {

    // Defaults for a new file
    uint32_t init_pool_cap  = 4 * 1024 * 1024;  // 4MB pool
    uint32_t init_table_cap = 65536;
    uint32_t init_id_cap    = 1024;
    size_t init_size = calc_mmap_size(init_pool_cap, init_table_cap, init_id_cap);

    // Open/create file via Platform
    auto handle = platform::mmap_open(file_path.c_str(), init_size);
    mmap_base_       = handle.data;
    mmap_total_size_ = handle.length;
    mmap_fd_         = handle.fd;

    auto* hdr = reinterpret_cast<StringPoolHeader*>(mmap_base_);

    bool is_new = (hdr->magic[0] != MAGIC[0] ||
                   hdr->magic[1] != MAGIC[1] ||
                   hdr->magic[2] != MAGIC[2] ||
                   hdr->magic[3] != MAGIC[3]);

    if (is_new) {
        // === FIRST OPEN ===
        std::memset(hdr, 0, sizeof(StringPoolHeader));
        std::memcpy(hdr->magic, MAGIC, 4);
        hdr->version             = FORMAT_VERSION;
        hdr->pool_size           = 0;
        hdr->pool_capacity       = init_pool_cap;
        hdr->next_id             = 1;
        hdr->table_size          = 0;
        hdr->table_capacity      = init_table_cap;
        hdr->id_to_offset_capacity = init_id_cap;

        update_pointers_from_mmap();

        // Zero-fill table
        for (uint32_t i = 0; i < table_capacity_; ++i) {
            table_[i].is_occupied = false;
        }
        // Zero-fill reverse lookup
        std::memset(id_to_offset_, 0, sizeof(uint32_t) * id_to_offset_capacity_);
    } else {
        // === REOPEN ===
        if (hdr->version != FORMAT_VERSION) {
            platform::MmapHandle h;
            h.data = mmap_base_; h.length = mmap_total_size_; h.fd = mmap_fd_;
            platform::mmap_close(h);
            throw std::runtime_error(
                "GraphLite: strings.gldb format version mismatch. "
                "Expected " + std::to_string(FORMAT_VERSION) +
                ", got " + std::to_string(hdr->version));
        }
        update_pointers_from_mmap();
    }
}

// ============================================================
// DESTRUCTOR
// ============================================================

StringPoolDictionary::~StringPoolDictionary() {
    if (is_mmap_mode_) {
        flush_header();
        platform::MmapHandle handle;
        handle.data = mmap_base_;
        handle.length = mmap_total_size_;
        handle.fd   = mmap_fd_;
        platform::mmap_sync(handle);
        platform::mmap_close(handle);
    } else {
        delete[] pool_;
        delete[] table_;
        delete[] id_to_offset_;
    }
}

// ============================================================
// PUBLIC API
// ============================================================

uint32_t StringPoolDictionary::get_or_create_id(const std::string& str) {
    // Rehash nếu load factor > 70%
    if (table_size_ * 10 >= table_capacity_ * 7) {
        rehash_table(table_capacity_ * 2);
    }

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

    // Chưa tồn tại — tạo mới
    uint32_t str_len = static_cast<uint32_t>(str.length());

    // Grow pool nếu cần
    if (pool_size_ + str_len + 1 > pool_capacity_) {
        resize_pool(pool_capacity_ * 2);
        // Recalculate index after potential remap
        index = hash_string(c_str) % table_capacity_;
        while (table_[index].is_occupied) {
            index = (index + 1) % table_capacity_;
        }
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
        // Recalculate index after potential remap
        index = hash_string(c_str) % table_capacity_;
        while (table_[index].is_occupied) {
            index = (index + 1) % table_capacity_;
        }
    }

    // Cập nhật reverse lookup
    id_to_offset_[new_id] = current_offset;

    // Insert vào hash table
    table_[index].pool_offset = current_offset;
    table_[index].id = new_id;
    table_[index].is_occupied = true;
    table_size_++;

    if (is_mmap_mode_) flush_header();

    return new_id;
}

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
    return 0;
}

std::string StringPoolDictionary::get_string(uint32_t id) const {
    if (id == 0 || id >= next_id_) return "";
    uint32_t offset = id_to_offset_[id];
    return std::string(pool_ + offset);
}

uint32_t StringPoolDictionary::get_id_count() const {
    return next_id_ - 1;
}

// ============================================================
// Internal Helper
// ============================================================

void StringPoolDictionary::sync() {
    if (!is_mmap_mode_) return;
    flush_header();
    platform::MmapHandle handle;
    handle.data = mmap_base_;
    handle.length = mmap_total_size_;
    handle.fd   = mmap_fd_;
    platform::mmap_sync(handle);
}

} // namespace utils
} // namespace graphlite
