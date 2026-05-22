#pragma once
#include <cstdint>
#include <cstddef>

namespace graphlite {
namespace utils {

// mô phỏng std::vector<uint8_t> nhưng tự quản lý bộ nhớ thủ công
class ByteBuffer {
private:
    uint8_t* data_;
    size_t size_;
    size_t capacity_;

    void reserve(size_t new_capacity);
public:
    ByteBuffer();

    ~ByteBuffer();

    ByteBuffer(const ByteBuffer&) = delete;
    ByteBuffer& operator=(const ByteBuffer&) = delete;

    void push_back(uint8_t value);

    uint8_t* data() const;
    size_t size() const;

    ByteBuffer(ByteBuffer&& other) noexcept;
    graphlite::utils::ByteBuffer& operator=(ByteBuffer&& other) noexcept;
};

} // namespace utils
} // namespace graphlite