/**
 * @file MiniQueue.h
 * @brief Hàng đợi động vòng (Dynamic Ring Buffer) không dùng STL.
 */

#pragma once

#include <cstddef>
#include <stdexcept>
#include <utility>

namespace graphlite {
namespace utils {

/**
 * @class MiniQueue
 * @brief Hàng đợi dùng Ring Buffer. Tự động resize khi đầy.
 */
template <typename T>
class MiniQueue {
private:
    T* data_;
    size_t capacity_;
    size_t head_;
    size_t tail_;
    size_t size_;

    void resize(size_t new_capacity) {
        T* new_data = new T[new_capacity];
        for (size_t i = 0; i < size_; ++i) {
            new_data[i] = std::move(data_[(head_ + i) % capacity_]);
        }
        delete[] data_;
        data_ = new_data;
        capacity_ = new_capacity;
        head_ = 0;
        tail_ = size_;
    }

public:
    MiniQueue(size_t initial_capacity = 16) 
        : capacity_(initial_capacity), head_(0), tail_(0), size_(0) {
        data_ = new T[capacity_];
    }

    ~MiniQueue() {
        delete[] data_;
    }

    // Move constructor
    MiniQueue(MiniQueue&& other) noexcept
        : data_(other.data_), capacity_(other.capacity_), 
          head_(other.head_), tail_(other.tail_), size_(other.size_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
        other.head_ = 0;
        other.tail_ = 0;
    }

    // Move assignment
    MiniQueue& operator=(MiniQueue&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            capacity_ = other.capacity_;
            head_ = other.head_;
            tail_ = other.tail_;
            size_ = other.size_;

            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
            other.head_ = 0;
            other.tail_ = 0;
        }
        return *this;
    }

    // Disable copy
    MiniQueue(const MiniQueue&) = delete;
    MiniQueue& operator=(const MiniQueue&) = delete;

    void push(const T& value) {
        if (size_ == capacity_) {
            resize(capacity_ == 0 ? 16 : capacity_ * 2);
        }
        data_[tail_] = value;
        tail_ = (tail_ + 1) % capacity_;
        size_++;
    }

    void push(T&& value) {
        if (size_ == capacity_) {
            resize(capacity_ == 0 ? 16 : capacity_ * 2);
        }
        data_[tail_] = std::move(value);
        tail_ = (tail_ + 1) % capacity_;
        size_++;
    }

    void pop() {
        if (empty()) return;
        head_ = (head_ + 1) % capacity_;
        size_--;
    }

    T& front() {
        return data_[head_];
    }

    const T& front() const {
        return data_[head_];
    }

    bool empty() const {
        return size_ == 0;
    }

    size_t size() const {
        return size_;
    }

    void clear() {
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }
};

} // namespace utils
} // namespace graphlite
