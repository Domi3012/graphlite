#include "ByteBuffer.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <stdexcept>
#include <cstdlib> // Cho std::malloc, std::realloc, std::free
#include <new>     // Cho std::bad_alloc

void graphlite::utils::ByteBuffer::reserve(size_t new_capacity) {
    if (new_capacity <= capacity_) return;
    
    uint8_t* new_data = static_cast<uint8_t*>(std::realloc(data_, new_capacity));
    
    if (new_data == nullptr) {
        throw std::bad_alloc(); // Tràn RAM, hệ điều hành từ chối cấp phát
    }
    
    data_ = new_data;
    capacity_ = new_capacity;
}

graphlite::utils::ByteBuffer::ByteBuffer() : size_(0), capacity_(16) {
    data_ = static_cast<uint8_t*>(std::malloc(capacity_));
    if (data_ == nullptr) {
        throw std::bad_alloc();
    }
}

graphlite::utils::ByteBuffer::~ByteBuffer() {
    std::free(data_);
}

// Chèn 1 byte vào cuối
void graphlite::utils::ByteBuffer::push_back(uint8_t value) {
    if (size_ == capacity_) {
        reserve(capacity_ * 2);
    }
    data_[size_++] = value;
}

// Getters cho các tầng khác đọc
uint8_t* graphlite::utils::ByteBuffer::data() const { return data_; }
size_t graphlite::utils::ByteBuffer::size() const { return size_; }


graphlite::utils::ByteBuffer::ByteBuffer(ByteBuffer&& other) noexcept 
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    // "Ăn cắp" quyền sở hữu con trỏ từ object cũ
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

// 3. CHO PHÉP gán bằng di chuyển (Move Assignment Operator)
graphlite::utils::ByteBuffer& graphlite::utils::ByteBuffer::operator=(ByteBuffer&& other) noexcept {
    if (this != &other) {
        std::free(data_); // Dọn dẹp vùng nhớ hiện tại của mình trước
        
        // "Ăn cắp" dữ liệu của thằng kia
        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        
        // Phế võ công thằng kia, để Destructor của nó không free nhầm
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }
    return *this;
}