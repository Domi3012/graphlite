#include "Platform.h"
#include <stdexcept>
#include <string>

namespace graphlite {
namespace platform {

// ----------------------------------------------------------
// POSIX Implementation (Linux, macOS, BSD)
// ----------------------------------------------------------
#if GRAPHLITE_PLATFORM_POSIX

MmapHandle mmap_open(const char* path, size_t initial_size) {
    MmapHandle handle;

    // Mở hoặc tạo file
    handle.fd = ::open(path, O_RDWR | O_CREAT, 0644);
    if (handle.fd == -1) {
        throw std::runtime_error(
            std::string("GraphLite: Cannot open file '") + path + "': " + std::strerror(errno));
    }

    // Đọc kích thước file hiện tại
    struct stat st;
    if (::fstat(handle.fd, &st) == -1) {
        ::close(handle.fd);
        throw std::runtime_error("GraphLite: fstat failed");
    }
    size_t file_size = static_cast<size_t>(st.st_size);

    // Nếu file mới (size = 0), mở rộng đến initial_size
    if (file_size == 0) {
        if (::ftruncate(handle.fd, static_cast<off_t>(initial_size)) == -1) {
            ::close(handle.fd);
            throw std::runtime_error("GraphLite: ftruncate failed during initialization");
        }
        file_size = initial_size;
    }

    // Map file vào bộ nhớ
    handle.data = ::mmap(nullptr, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, handle.fd, 0);
    if (handle.data == MAP_FAILED) {
        ::close(handle.fd);
        throw std::runtime_error("GraphLite: mmap failed");
    }
    handle.length = file_size;

    return handle;
}

void mmap_grow(MmapHandle& handle, size_t new_size) {
    if (new_size <= handle.length) return;

    // Unmap vùng cũ
    if (handle.data && handle.data != MAP_FAILED) {
        ::munmap(handle.data, handle.length);
    }

    // Mở rộng file
    if (::ftruncate(handle.fd, static_cast<off_t>(new_size)) == -1) {
        throw std::runtime_error("GraphLite: ftruncate failed during grow");
    }

    // Remap với kích thước mới
    handle.data = ::mmap(nullptr, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, handle.fd, 0);
    if (handle.data == MAP_FAILED) {
        throw std::runtime_error("GraphLite: mmap failed during grow");
    }
    handle.length = new_size;
}

void mmap_sync(MmapHandle& handle) {
    if (handle.data && handle.data != MAP_FAILED && handle.length > 0) {
        ::msync(handle.data, handle.length, MS_SYNC);
    }
}

void mmap_close(MmapHandle& handle) {
    if (handle.data && handle.data != MAP_FAILED) {
        ::msync(handle.data, handle.length, MS_SYNC);
        ::munmap(handle.data, handle.length);
        handle.data = nullptr;
    }
    if (handle.fd != -1) {
        ::close(handle.fd);
        handle.fd = -1;
    }
    handle.length = 0;
}

// ----------------------------------------------------------
// WINDOWS Implementation
// ----------------------------------------------------------
#elif GRAPHLITE_PLATFORM_WINDOWS

MmapHandle mmap_open(const char* path, size_t initial_size) {
    MmapHandle handle;

    // Mở hoặc tạo file
    handle.file_handle = CreateFileA(
        path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (handle.file_handle == INVALID_HANDLE_VALUE) {
        throw std::runtime_error(
            std::string("GraphLite: Cannot open file '") + path + "'");
    }

    // Đọc kích thước file hiện tại
    LARGE_INTEGER file_size_li;
    if (!GetFileSizeEx(handle.file_handle, &file_size_li)) {
        CloseHandle(handle.file_handle);
        throw std::runtime_error("GraphLite: GetFileSizeEx failed");
    }
    size_t file_size = static_cast<size_t>(file_size_li.QuadPart);

    // Nếu file mới, mở rộng đến initial_size
    if (file_size == 0) {
        LARGE_INTEGER li;
        li.QuadPart = static_cast<LONGLONG>(initial_size);
        if (!SetFilePointerEx(handle.file_handle, li, nullptr, FILE_BEGIN)) {
            CloseHandle(handle.file_handle);
            throw std::runtime_error("GraphLite: SetFilePointerEx failed");
        }
        if (!SetEndOfFile(handle.file_handle)) {
            CloseHandle(handle.file_handle);
            throw std::runtime_error("GraphLite: SetEndOfFile failed");
        }
        file_size = initial_size;
    }

    // Tạo file mapping
    DWORD size_high = static_cast<DWORD>(file_size >> 32);
    DWORD size_low  = static_cast<DWORD>(file_size & 0xFFFFFFFF);
    handle.mapping_handle = CreateFileMappingA(
        handle.file_handle,
        nullptr,
        PAGE_READWRITE,
        size_high,
        size_low,
        nullptr);

    if (!handle.mapping_handle) {
        CloseHandle(handle.file_handle);
        throw std::runtime_error("GraphLite: CreateFileMapping failed");
    }

    // Map view
    handle.data = MapViewOfFile(
        handle.mapping_handle,
        FILE_MAP_ALL_ACCESS,
        0, 0, 0);

    if (!handle.data) {
        CloseHandle(handle.mapping_handle);
        CloseHandle(handle.file_handle);
        throw std::runtime_error("GraphLite: MapViewOfFile failed");
    }
    handle.length = file_size;

    return handle;
}

void mmap_grow(MmapHandle& handle, size_t new_size) {
    if (new_size <= handle.length) return;

    // Unmap view cũ
    if (handle.data) {
        FlushViewOfFile(handle.data, 0);
        UnmapViewOfFile(handle.data);
        handle.data = nullptr;
    }

    // Đóng mapping cũ
    if (handle.mapping_handle) {
        CloseHandle(handle.mapping_handle);
        handle.mapping_handle = nullptr;
    }

    // Mở rộng file
    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(new_size);
    if (!SetFilePointerEx(handle.file_handle, li, nullptr, FILE_BEGIN)) {
        throw std::runtime_error("GraphLite: SetFilePointerEx failed during grow");
    }
    if (!SetEndOfFile(handle.file_handle)) {
        throw std::runtime_error("GraphLite: SetEndOfFile failed during grow");
    }

    // Tạo mapping mới
    DWORD size_high = static_cast<DWORD>(new_size >> 32);
    DWORD size_low  = static_cast<DWORD>(new_size & 0xFFFFFFFF);
    handle.mapping_handle = CreateFileMappingA(
        handle.file_handle, nullptr, PAGE_READWRITE,
        size_high, size_low, nullptr);

    if (!handle.mapping_handle) {
        throw std::runtime_error("GraphLite: CreateFileMapping failed during grow");
    }

    // Map lại
    handle.data = MapViewOfFile(
        handle.mapping_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);

    if (!handle.data) {
        CloseHandle(handle.mapping_handle);
        throw std::runtime_error("GraphLite: MapViewOfFile failed during grow");
    }
    handle.length = new_size;
}

void mmap_sync(MmapHandle& handle) {
    if (handle.data) {
        FlushViewOfFile(handle.data, 0);
        FlushFileBuffers(handle.file_handle);
    }
}

void mmap_close(MmapHandle& handle) {
    if (handle.data) {
        FlushViewOfFile(handle.data, 0);
        UnmapViewOfFile(handle.data);
        handle.data = nullptr;
    }
    if (handle.mapping_handle) {
        CloseHandle(handle.mapping_handle);
        handle.mapping_handle = nullptr;
    }
    if (handle.file_handle != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(handle.file_handle);
        CloseHandle(handle.file_handle);
        handle.file_handle = INVALID_HANDLE_VALUE;
    }
    handle.length = 0;
}

#endif // GRAPHLITE_PLATFORM_*

} // namespace platform
} // namespace graphlite
