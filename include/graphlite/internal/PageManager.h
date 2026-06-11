/**
 * @file PageManager.h
 * @brief Quản lý cấp phát/thu hồi Page trên mmap file (edges.gldb).
 * @version 1.0
 * 
 * Cung cấp:
 * - Page allocation/deallocation với free-list
 * - Slot-level management bên trong mỗi page
 * - Direct pointer access tới DiskEdge trên mmap
 * 
 * Page Layout (4096 bytes):
 *   [PageHeader: 64B] [DiskEdge slots: 126 × 32B = 4032B]
 * 
 * @note Sẽ implement đầy đủ ở Phase 2.
 */

#pragma once
#include "MmapFile.h"
#include "../types.h"

namespace graphlite {
namespace internal {

// class PageManager — sẽ implement ở Phase 2

} // namespace internal
} // namespace graphlite
