/**
 * @file MiniVector.h
 * @brief Container mảng động tối giản — thay thế std::vector, zero-overhead.
 * @version 1.0
 *
 * Thiết kế cho hiệu năng tối đa:
 * - Manual memory management (new/delete, không dùng allocator)
 * - Move semantics, chặn copy
 * - Pointer-based iterators (tương thích range-for)
 * - Geometric growth (×2)
 *
 * @note Đây là template class → LUÔN inline, không cần hybrid macro guard.
 */

#pragma once
#include <cstdint>
#include <cstddef>
#include <utility>
#include <stdexcept>

namespace graphlite {
namespace utils {

template <typename T>
class MiniVector {
private:
    T*     data_;
    size_t size_;
    size_t capacity_;

    void reserve(size_t new_capacity) {
        if (new_capacity <= capacity_) return;
        T* new_data = new T[new_capacity];
        for (size_t i = 0; i < size_; ++i) {
            new_data[i] = std::move(data_[i]);
        }
        delete[] data_;
        data_ = new_data;
        capacity_ = new_capacity;
    }

public:
    // ==========================================
    // CONSTRUCTORS & DESTRUCTOR
    // ==========================================

    /** @brief Constructor mặc định — cấp phát sẵn 16 phần tử. */
    MiniVector() : data_(nullptr), size_(0), capacity_(16) {
        data_ = new T[capacity_];
    }

    /**
     * @brief Constructor với kích thước ban đầu.
     * @param initial_size Số phần tử cần khởi tạo.
     * @param value Giá trị mặc định cho mỗi phần tử.
     */
    MiniVector(size_t initial_size, const T& value = T{})
        : data_(nullptr), size_(0), capacity_(initial_size > 0 ? initial_size : 16) {
        data_ = new T[capacity_];
        for (size_t i = 0; i < initial_size; ++i) {
            data_[i] = value;
        }
        size_ = initial_size;
    }

    ~MiniVector() {
        delete[] data_;
    }

    // ==========================================
    // MOVE SEMANTICS (Cướp tài nguyên, không copy)
    // ==========================================

    MiniVector(MiniVector&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    MiniVector& operator=(MiniVector&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_     = other.data_;
            size_     = other.size_;
            capacity_ = other.capacity_;
            other.data_     = nullptr;
            other.size_     = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    // ==========================================
    // SAFETY: Chặn Copy
    // ==========================================

    MiniVector(const MiniVector&) = delete;
    MiniVector& operator=(const MiniVector&) = delete;

    // ==========================================
    // MODIFIERS
    // ==========================================

    void push_back(const T& value) {
        if (size_ == capacity_) reserve(capacity_ * 2);
        data_[size_++] = value;
    }

    void push_back(T&& value) {
        if (size_ == capacity_) reserve(capacity_ * 2);
        data_[size_++] = std::move(value);
    }

    void pop_back() {
        if (size_ > 0) size_--;
    }

    /** @brief Reset size về 0 — tái sử dụng RAM, không giải phóng. */
    void clear() {
        size_ = 0;
    }

    /**
     * @brief Thay đổi kích thước mảng.
     * Nếu new_size > size hiện tại → thêm phần tử mới với giá trị value.
     * Nếu new_size < size → chỉ cắt bớt (không giải phóng).
     */
    void resize(size_t new_size, const T& value = T{}) {
        if (new_size > capacity_) reserve(new_size);
        for (size_t i = size_; i < new_size; ++i) {
            data_[i] = value;
        }
        size_ = new_size;
    }

    // ==========================================
    // ACCESSORS
    // ==========================================

    T*       data()       { return data_; }
    const T* data() const { return data_; }
    size_t   size()     const { return size_; }
    size_t   capacity() const { return capacity_; }
    bool     empty()    const { return size_ == 0; }

    T&       operator[](size_t index)       { return data_[index]; }
    const T& operator[](size_t index) const { return data_[index]; }

    T&       back()       { return data_[size_ - 1]; }
    const T& back() const { return data_[size_ - 1]; }

    // ==========================================
    // ITERATORS (Pointer-based, zero overhead)
    // Cho phép sử dụng range-for: for (auto& x : vec)
    // ==========================================

    T*       begin()        { return data_; }
    T*       end()          { return data_ + size_; }
    const T* begin()  const { return data_; }
    const T* end()    const { return data_ + size_; }
    const T* cbegin() const { return data_; }
    const T* cend()   const { return data_ + size_; }
};

} // namespace utils
} // namespace graphlite
