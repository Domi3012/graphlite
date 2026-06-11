/**
 * @file ClockCache.h
 * @brief CLOCK eviction cache cho hot node edge lists.
 * @version 1.0
 * 
 * Thuật toán CLOCK (Second-Chance):
 * - Mỗi entry có reference bit
 * - Kim đồng hồ quay vòng, skip entry có ref=1 (reset về 0)
 * - Evict entry đầu tiên có ref=0
 * - Ưu điểm so với LRU: Không cần maintain doubly-linked list → ít overhead hơn
 * 
 * @note Sẽ implement đầy đủ ở Phase 3.
 */

#pragma once
#include <cstdint>
#include <cstddef>

namespace graphlite {
namespace internal {

// template <typename Key, typename Value>
// class ClockCache — sẽ implement ở Phase 3

} // namespace internal
} // namespace graphlite
