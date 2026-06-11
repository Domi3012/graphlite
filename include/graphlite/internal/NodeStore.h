/**
 * @file NodeStore.h
 * @brief Flat array lưu trữ NodeRecord trên mmap (nodes.gldb).
 * @version 1.0
 * 
 * Cung cấp:
 * - O(1) lookup: NodeRecord* record = &records[node_id]
 * - Mmap-backed: Dữ liệu nằm trên đĩa, OS tự quản lý page cache
 * - Tự động grow khi NodeID vượt quá capacity
 * 
 * @note Sẽ implement đầy đủ ở Phase 2.
 */

#pragma once
#include "MmapFile.h"
#include "../types.h"

namespace graphlite {
namespace internal {

// class NodeStore — sẽ implement ở Phase 2

} // namespace internal
} // namespace graphlite
