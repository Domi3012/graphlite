/**
 * @file Platform.h
 * @brief Cross-platform abstraction layer — OS detection + mmap primitives.
 * @version 1.0
 *
 * Phát hiện hệ điều hành tại compile-time và cung cấp unified API cho:
 * - Memory-mapped files (mmap / CreateFileMapping)
 * - File size management (ftruncate / SetEndOfFile)
 * - Memory sync (msync / FlushViewOfFile)
 *
 * Dùng hybrid macro pattern: compiled mode hoặc header-only.
 */

#pragma once

// ============================================================
// OS DETECTION
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
// PLATFORM HEADERS
// ============================================================

#if GRAPHLITE_PLATFORM_POSIX
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <cerrno>
#elif GRAPHLITE_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

#include <graphlite/types.h> // Cho GRAPHLITE_FUNC, GRAPHLITE_IMPL_GUARD

namespace graphlite {
namespace platform {

// ============================================================
// CONSTANTS
// ============================================================

/** @brief Kích thước page chuẩn (trùng OS page size). */
constexpr size_t PAGE_SIZE = 4096;

// ============================================================
// MMAP HANDLE
// ============================================================

/**
 * @struct MmapHandle
 * @brief Opaque handle cho một vùng nhớ được map từ file.
 *
 * Chứa thông tin platform-specific cần thiết cho mmap operations.
 * Đây là cấu trúc nội bộ — user không trực tiếp tương tác.
 */
struct MmapHandle {
    void*  data;       ///< Con trỏ tới vùng nhớ đã map.
    size_t length;     ///< Kích thước hiện tại của mapping (bytes).

#if GRAPHLITE_PLATFORM_POSIX
    int fd;            ///< File descriptor (POSIX).
#elif GRAPHLITE_PLATFORM_WINDOWS
    HANDLE file_handle;    ///< File handle (Windows).
    HANDLE mapping_handle; ///< File mapping handle (Windows).
#endif

    /** @brief Constructor — khởi tạo handle trống. */
    MmapHandle() : data(nullptr), length(0)
#if GRAPHLITE_PLATFORM_POSIX
        , fd(-1)
#elif GRAPHLITE_PLATFORM_WINDOWS
        , file_handle(INVALID_HANDLE_VALUE), mapping_handle(nullptr)
#endif
    {}
};

// ============================================================
// API DECLARATIONS
// ============================================================

/**
 * @brief Mở/tạo file và map vào bộ nhớ.
 * @param path Đường dẫn file.
 * @param initial_size Kích thước khởi tạo nếu file mới (bytes).
 * @return MmapHandle chứa con trỏ tới vùng nhớ đã map.
 * @throws std::runtime_error nếu thất bại.
 */
MmapHandle mmap_open(const char* path, size_t initial_size);

/**
 * @brief Mở rộng file và remap vùng nhớ.
 * @param handle Handle hiện tại (sẽ được cập nhật).
 * @param new_size Kích thước mới (phải >= handle.length).
 * @throws std::runtime_error nếu thất bại.
 */
void mmap_grow(MmapHandle& handle, size_t new_size);

/**
 * @brief Flush dirty pages xuống ổ đĩa vật lý.
 * @param handle Handle cần sync.
 */
void mmap_sync(MmapHandle& handle);

/**
 * @brief Unmap vùng nhớ và đóng file.
 * @param handle Handle cần đóng (sẽ reset về trạng thái trống).
 */
void mmap_close(MmapHandle& handle);

// ============================================================
// IMPLEMENTATION (chỉ compile khi header-only hoặc trong src/graphlite.cpp)
// ============================================================

} // namespace platform
} // namespace graphlite
