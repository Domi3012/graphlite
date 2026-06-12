/**
 * @file MmapFile.h
 * @brief RAII wrapper cho memory-mapped files — xây dựng trên Platform.h.
 * @version 1.0
 *
 * Cung cấp:
 * - Tự động open/close với RAII (destructor sync + close)
 * - Auto-grow khi cần thêm dung lượng
 * - Typed pointer access: as<T>() và at<T>(offset) — zero-copy
 * - Move semantics, chặn copy
 * - Cross-platform thông qua Platform.h
 *
 * Dùng hybrid macro pattern (GRAPHLITE_IMPL_GUARD).
 */

#pragma once
#include "Platform.h"
#include <string>
#include <utility>

namespace graphlite {
namespace internal {

/**
 * @class MmapFile
 * @brief RAII wrapper quản lý vòng đời của một memory-mapped file.
 *
 * Sử dụng:
 * @code
 * MmapFile file("data.gldb", 1024 * 1024);  // 1MB initial
 * auto* header = file.as<FileHeader>();       // Typed access
 * auto* record = file.at<NodeRecord>(64);     // Access at offset
 * file.grow(2 * 1024 * 1024);                 // Grow to 2MB
 * file.sync();                                 // Flush to disk
 * // Destructor tự động sync + close
 * @endcode
 */
class MmapFile {
private:
    platform::MmapHandle handle_;
    size_t capacity_;  ///< Kích thước hiện tại (aligned to PAGE_SIZE)

public:
    /**
     * @brief Mở/tạo file và map vào bộ nhớ.
     * @param path Đường dẫn file.
     * @param initial_size Kích thước khởi tạo nếu file mới (default: 256 pages = 1MB).
     */
    explicit MmapFile(const std::string& path, 
                      size_t initial_size = platform::PAGE_SIZE * 256);

    /** @brief Destructor — sync + close file tự động. */
    ~MmapFile();

    // ==========================================
    // ACCESSORS
    // ==========================================

    /** @brief Con trỏ thô tới vùng nhớ đã map. */
    void*       data()       { return handle_.data; }
    const void* data() const { return handle_.data; }

    /** @brief Kích thước hiện tại của file (bytes). */
    size_t capacity() const { return capacity_; }

    /**
     * @brief Truy cập typed — cast toàn bộ vùng nhớ thành kiểu T*.
     * @tparam T Kiểu dữ liệu đích.
     * @return Con trỏ T* trỏ vào đầu file.
     */
    template <typename T>
    T* as() { return static_cast<T*>(handle_.data); }

    template <typename T>
    const T* as() const { return static_cast<const T*>(handle_.data); }

    /**
     * @brief Truy cập typed tại offset cụ thể.
     * @tparam T Kiểu dữ liệu đích.
     * @param byte_offset Vị trí (byte) trong file.
     * @return Con trỏ T* trỏ vào vị trí offset.
     */
    template <typename T>
    T* at(size_t byte_offset) {
        return reinterpret_cast<T*>(static_cast<uint8_t*>(handle_.data) + byte_offset);
    }

    template <typename T>
    const T* at(size_t byte_offset) const {
        return reinterpret_cast<const T*>(static_cast<const uint8_t*>(handle_.data) + byte_offset);
    }

    // ==========================================
    // OPERATIONS
    // ==========================================

    /**
     * @brief Mở rộng file + remap.
     * @param new_size Kích thước mới (bytes). Sẽ align lên bội số PAGE_SIZE.
     * @note Con trỏ data() CÓ THỂ THAY ĐỔI sau lệnh grow.
     */
    void grow(size_t new_size);

    /** @brief Flush dirty pages xuống ổ đĩa vật lý. */
    void sync();

    // ==========================================
    // OWNERSHIP
    // ==========================================

    // Chặn copy — mỗi MmapFile sở hữu duy nhất fd/handle
    MmapFile(const MmapFile&) = delete;
    MmapFile& operator=(const MmapFile&) = delete;

    // Move semantics
    MmapFile(MmapFile&& other) noexcept;
    MmapFile& operator=(MmapFile&& other) noexcept;
};

// ============================================================
// IMPLEMENTATION
// ============================================================

} // namespace internal
} // namespace graphlite
