#pragma once
#include <cstdint>
#include <cstddef>
#include <utility>    // Cho std::move
#include <stdexcept>

namespace graphlite {
namespace utils {

template <typename T>
class MiniVector {
private:
    T* data_;
    size_t size_;
    size_t capacity_;

    void reserve(size_t new_capacity) {
        if (new_capacity <= capacity_) return;
        T* new_data = new T[new_capacity];
        for (size_t i = 0; i < size_; ++i) {
            new_data[i] = std::move(data_[i]); // Ép dùng Move Semantics
        }
        delete[] data_;
        data_ = new_data;
        capacity_ = new_capacity;
    }

public:
    MiniVector() : size_(0), capacity_(16) {
        data_ = new T[capacity_];
    }

    ~MiniVector() {
        delete[] data_;
    }

    // Move Semantics: Cướp tài nguyên, không copy
    MiniVector(MiniVector&& other) noexcept 
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    MiniVector& operator=(MiniVector&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    // Chặn Copy (Đảm bảo an toàn bộ nhớ tuyệt đối)
    MiniVector(const MiniVector&) = delete;
    MiniVector& operator=(const MiniVector&) = delete;

    void push_back(const T& value) {
        if (size_ == capacity_) reserve(capacity_ * 2);
        data_[size_++] = value;
    }

    void push_back(T&& value) {
        if (size_ == capacity_) reserve(capacity_ * 2);
        data_[size_++] = std::move(value);
    }

    void clear() {
        size_ = 0; // Tái sử dụng RAM, không free()
    }

    T* data() { return data_; }
    const T* data() const { return data_; }
    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }

    // Dùng mảng động tiện lợi như mảng thường
    T& operator[](size_t index) { return data_[index]; }
    const T& operator[](size_t index) const { return data_[index]; }
};

} // namespace utils
} // namespace graphlite