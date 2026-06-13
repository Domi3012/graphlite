/**
 * @file ClockCache.h
 * @brief CLOCK eviction cache — Second-Chance algorithm cho hot data.
 * @version 1.0
 *
 * Thuật toán CLOCK (Second-Chance):
 * - Circular buffer chứa cache entries
 * - Mỗi entry có reference bit (ref_bit)
 * - Khi cần evict: kim đồng hồ (hand_) quay vòng
 *   - Nếu ref_bit = 1 → reset về 0 (second chance), đi tiếp
 *   - Nếu ref_bit = 0 → evict entry này
 * - Khi access (lookup hit): set ref_bit = 1
 *
 * Ưu điểm so với LRU:
 * - Ít pointer writes hơn (1 bit set vs move-to-head)
 * - Eviction quality gần bằng LRU trong thực tế
 * - Phù hợp cho graph traversal với hàng triệu lookups
 *
 * Template parameters:
 * - Value: Kiểu giá trị cache (e.g., MiniVector<GenericEdge>)
 * - Key mặc định là uint32_t (NodeID)
 *
 * @note Header-only vì template class.
 */

#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <utility>
#include "NumericHashMap.h"

namespace graphlite {
namespace internal {

/**
 * @class ClockCache
 * @brief CLOCK eviction cache: uint32_t Key → Value.
 *
 * @tparam Value Kiểu giá trị. Phải hỗ trợ move semantics.
 *
 * Sử dụng:
 * @code
 * ClockCache<MiniVector<GenericEdge>> cache(8192);
 * 
 * // Insert
 * cache.insert(node_id, std::move(edge_list));
 * 
 * // Lookup
 * auto* cached = cache.lookup(node_id);
 * if (cached) { // HIT
 *     return *cached;
 * }
 * // MISS → load from disk, then insert
 * @endcode
 */
template <typename Value>
class ClockCache {
private:
    struct Entry {
        uint32_t key;
        Value    value;
        bool     occupied;  ///< Slot có dữ liệu?
        bool     ref_bit;   ///< Second-chance bit (1 = recently used)
    };

    Entry*    entries_;     ///< Circular buffer
    uint32_t  capacity_;   ///< Kích thước cố định
    uint32_t  size_;       ///< Số entries đang occupied
    uint32_t  hand_;       ///< Kim đồng hồ — vị trí eviction kế tiếp

    utils::NumericHashMap index_;  ///< Key → slot index mapping

    /**
     * @brief Tìm slot trống hoặc evict bằng CLOCK algorithm.
     * @return Index của slot có thể dùng.
     */
    uint32_t findEvictSlot() {
        // Quay kim đồng hồ tìm victim
        while (true) {
            Entry& e = entries_[hand_];
            if (!e.occupied) {
                // Slot trống → dùng luôn
                uint32_t slot = hand_;
                hand_ = (hand_ + 1) % capacity_;
                return slot;
            }
            if (!e.ref_bit) {
                // ref_bit = 0 → evict entry này
                index_.remove(static_cast<uint64_t>(e.key));
                e.occupied = false;
                size_--;
                uint32_t slot = hand_;
                hand_ = (hand_ + 1) % capacity_;
                return slot;
            }
            // ref_bit = 1 → give second chance, reset to 0
            e.ref_bit = false;
            hand_ = (hand_ + 1) % capacity_;
        }
    }

public:
    /**
     * @brief Constructor.
     * @param capacity Số entries tối đa (cố định). Default: 8192.
     */
    explicit ClockCache(uint32_t capacity = 8192)
        : capacity_(capacity), size_(0), hand_(0), index_(capacity * 2) {
        entries_ = new Entry[capacity_];
        for (uint32_t i = 0; i < capacity_; ++i) {
            entries_[i].occupied = false;
            entries_[i].ref_bit = false;
        }
    }

    ~ClockCache() {
        delete[] entries_;
    }

    // Chặn copy
    ClockCache(const ClockCache&) = delete;
    ClockCache& operator=(const ClockCache&) = delete;

    // Move semantics
    ClockCache(ClockCache&& other) noexcept
        : entries_(other.entries_), capacity_(other.capacity_),
          size_(other.size_), hand_(other.hand_),
          index_(std::move(other.index_)) {
        other.entries_ = nullptr;
        other.capacity_ = 0;
        other.size_ = 0;
    }

    // ==========================================
    // CORE OPERATIONS
    // ==========================================

    /**
     * @brief Tra cứu value theo key — O(1) amortized.
     * @param key Key cần tìm.
     * @return Pointer tới value nếu HIT, nullptr nếu MISS.
     *
     * @note Khi HIT, tự động set ref_bit = 1 (đánh dấu recently used).
     *       Pointer trả về valid cho đến khi insert() hoặc invalidate() tiếp theo.
     */
    Value* lookup(uint32_t key) {
        size_t slot_idx;
        if (!index_.get(static_cast<uint64_t>(key), slot_idx)) {
            return nullptr;  // MISS
        }

        Entry& e = entries_[static_cast<uint32_t>(slot_idx)];
        if (!e.occupied || e.key != key) {
            // Stale index entry (shouldn't happen, but defensive)
            return nullptr;
        }

        e.ref_bit = true;  // Mark as recently used
        return &e.value;
    }

    /**
     * @brief Chèn entry mới (hoặc cập nhật nếu key đã tồn tại).
     * @param key Key.
     * @param value Value (move vào cache).
     *
     * Nếu cache đầy → CLOCK eviction chọn victim.
     */
    void insert(uint32_t key, Value value) {
        // Kiểm tra key đã tồn tại chưa
        size_t existing_slot;
        if (index_.get(static_cast<uint64_t>(key), existing_slot)) {
            Entry& e = entries_[static_cast<uint32_t>(existing_slot)];
            if (e.occupied && e.key == key) {
                // Update in-place
                e.value = std::move(value);
                e.ref_bit = true;
                return;
            }
        }

        // Tìm slot (evict nếu cần)
        uint32_t slot = findEvictSlot();

        // Insert
        Entry& e = entries_[slot];
        e.key = key;
        e.value = std::move(value);
        e.occupied = true;
        e.ref_bit = true;
        size_++;

        index_.put(static_cast<uint64_t>(key), static_cast<size_t>(slot));
    }

    /**
     * @brief Xóa entry theo key (invalidation).
     * @param key Key cần xóa.
     * @return true nếu đã xóa, false nếu không tìm thấy.
     *
     * Dùng khi addEdge() thay đổi dữ liệu → cache entry cũ stale.
     */
    bool invalidate(uint32_t key) {
        size_t slot_idx;
        if (!index_.get(static_cast<uint64_t>(key), slot_idx)) {
            return false;
        }

        Entry& e = entries_[static_cast<uint32_t>(slot_idx)];
        if (e.occupied && e.key == key) {
            e.occupied = false;
            e.ref_bit = false;
            size_--;
            index_.remove(static_cast<uint64_t>(key));
            return true;
        }
        return false;
    }

    /**
     * @brief Xóa toàn bộ cache.
     */
    void clear() {
        for (uint32_t i = 0; i < capacity_; ++i) {
            entries_[i].occupied = false;
            entries_[i].ref_bit = false;
        }
        size_ = 0;
        hand_ = 0;
        // NumericHashMap doesn't have clear(), so rebuild
        // (index_ entries become stale — lookups will miss, which is safe)
    }

    // ==========================================
    // METADATA
    // ==========================================

    /** @brief Số entries đang trong cache. */
    uint32_t size() const { return size_; }

    /** @brief Dung lượng tối đa. */
    uint32_t capacity() const { return capacity_; }

    /** @brief Cache đầy chưa? */
    bool full() const { return size_ >= capacity_; }
};

} // namespace internal
} // namespace graphlite
