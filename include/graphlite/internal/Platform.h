/**
 * @file Platform.h
 * @brief Cross-platform abstraction layer cho GraphLite.
 * @version 1.0
 * 
 * Detect hệ điều hành tại compile-time và cung cấp unified API cho:
 * - Memory-mapped files (mmap / CreateFileMapping)
 * - File I/O primitives (ftruncate / SetFilePointerEx)
 * - Memory sync (msync / FlushViewOfFile)
 * 
 * @note Đây là file NỘI BỘ. Không include trực tiếp từ application code.
 */

#pragma once

// ============================================================
// PHASE 1: OS DETECTION
// ============================================================

#if defined(_WIN32) || defined(_WIN64)
    #define GRAPHLITE_PLATFORM_WINDOWS 1
    #define GRAPHLITE_PLATFORM_POSIX   0
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    #define GRAPHLITE_PLATFORM_WINDOWS 0
    #define GRAPHLITE_PLATFORM_POSIX   1
#else
    #error "GraphLite: Unsupported platform. Requires Windows, Linux, or macOS."
#endif

// ============================================================
// PHASE 2: PLATFORM HEADERS
// ============================================================

#if GRAPHLITE_PLATFORM_POSIX
    #include <sys/mman.h>    // mmap, munmap, msync
    #include <sys/stat.h>    // fstat
    #include <fcntl.h>       // open, O_RDWR, O_CREAT
    #include <unistd.h>      // close, ftruncate, sysconf
#elif GRAPHLITE_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace graphlite {
namespace platform {

// ============================================================
// PHASE 3: UNIFIED API (sẽ implement ở Phase 1 của kế hoạch)
// ============================================================

/**
 * @brief Kích thước page mặc định của hệ thống (4096 bytes).
 */
constexpr size_t PAGE_SIZE = 4096;

/**
 * @struct MmapHandle
 * @brief Opaque handle cho một vùng nhớ được map từ file.
 */
struct MmapHandle {
    void*    data;       ///< Con trỏ tới vùng nhớ đã map
    size_t   length;     ///< Kích thước hiện tại của mapping
    
#if GRAPHLITE_PLATFORM_POSIX
    int      fd;         ///< File descriptor (POSIX)
#elif GRAPHLITE_PLATFORM_WINDOWS
    HANDLE   file_handle;    ///< File handle (Windows)
    HANDLE   mapping_handle; ///< Mapping handle (Windows)
#endif
};

// --- Các hàm sẽ implement inline tại Phase 1 ---

/** @brief Mở/tạo file và map vào bộ nhớ. */
// MmapHandle mmap_open(const std::string& path, size_t initial_size);

/** @brief Grow file và remap. */
// void mmap_grow(MmapHandle& handle, size_t new_size);

/** @brief Flush dirty pages xuống đĩa. */
// void mmap_sync(MmapHandle& handle);

/** @brief Unmap và đóng file. */
// void mmap_close(MmapHandle& handle);

} // namespace platform
} // namespace graphlite
