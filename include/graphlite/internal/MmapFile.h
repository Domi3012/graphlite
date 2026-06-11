/**
 * @file MmapFile.h
 * @brief RAII wrapper cho memory-mapped file, xây dựng trên Platform.h.
 * @version 1.0
 * 
 * Cung cấp:
 * - Auto open/close với RAII
 * - Auto-grow file khi cần thêm dung lượng  
 * - Typed pointer access (zero-copy read/write)
 * - Cross-platform thông qua Platform.h
 * 
 * @note Sẽ implement đầy đủ ở Phase 1.
 */

#pragma once
#include "Platform.h"

namespace graphlite {
namespace internal {

// class MmapFile — sẽ implement ở Phase 1

} // namespace internal
} // namespace graphlite
