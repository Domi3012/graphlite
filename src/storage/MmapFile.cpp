#include "MmapFile.h"
#include "Platform.h"
#include <stdexcept>

namespace graphlite {
namespace internal {

MmapFile::MmapFile(const std::string& path, size_t initial_size) {
    // Align initial_size lên bội số PAGE_SIZE
    if (initial_size % platform::PAGE_SIZE != 0) {
        initial_size = ((initial_size / platform::PAGE_SIZE) + 1) * platform::PAGE_SIZE;
    }
    handle_ = platform::mmap_open(path.c_str(), initial_size);
    capacity_ = handle_.length;
}

MmapFile::~MmapFile() {
    if (handle_.data) {
        platform::mmap_close(handle_);
    }
}

void MmapFile::grow(size_t new_size) {
    // Align lên bội số PAGE_SIZE
    if (new_size % platform::PAGE_SIZE != 0) {
        new_size = ((new_size / platform::PAGE_SIZE) + 1) * platform::PAGE_SIZE;
    }
    if (new_size <= capacity_) return;

    platform::mmap_grow(handle_, new_size);
    capacity_ = handle_.length;
}

void MmapFile::sync() {
    platform::mmap_sync(handle_);
}

MmapFile::MmapFile(MmapFile&& other) noexcept
    : handle_(other.handle_), capacity_(other.capacity_) {
    other.handle_.data = nullptr;
    other.handle_.length = 0;
#if GRAPHLITE_PLATFORM_POSIX
    other.handle_.fd = -1;
#elif GRAPHLITE_PLATFORM_WINDOWS
    other.handle_.file_handle = INVALID_HANDLE_VALUE;
    other.handle_.mapping_handle = nullptr;
#endif
    other.capacity_ = 0;
}

MmapFile& MmapFile::operator=(MmapFile&& other) noexcept {
    if (this != &other) {
        // Đóng file hiện tại
        if (handle_.data) {
            platform::mmap_close(handle_);
        }
        // Cướp tài nguyên
        handle_ = other.handle_;
        capacity_ = other.capacity_;
        // Vô hiệu hóa source
        other.handle_.data = nullptr;
        other.handle_.length = 0;
#if GRAPHLITE_PLATFORM_POSIX
        other.handle_.fd = -1;
#elif GRAPHLITE_PLATFORM_WINDOWS
        other.handle_.file_handle = INVALID_HANDLE_VALUE;
        other.handle_.mapping_handle = nullptr;
#endif
        other.capacity_ = 0;
    }
    return *this;
}

} // namespace internal
} // namespace graphlite
