#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

namespace graphlite {
namespace utils {

// 1. Ghi một số nguyên 32-bit (4 bytes) vào cuối buffer
inline void writeUint32(std::vector<uint8_t>& buffer, uint32_t value) {
    // Ép kiểu Little-Endian: Byte nhỏ nhất vào trước
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

// 2. Đọc một số nguyên 32-bit từ buffer và tự động tăng offset
inline uint32_t readUint32(const std::vector<uint8_t>& buffer, size_t& offset) {
    if (offset + 4 > buffer.size()) {
        throw std::out_of_range("Buffer overflow khi đọc uint32_t");
    }
    uint32_t value = static_cast<uint32_t>(buffer[offset]) |
                    (static_cast<uint32_t>(buffer[offset + 1]) << 8) |
                    (static_cast<uint32_t>(buffer[offset + 2]) << 16) |
                    (static_cast<uint32_t>(buffer[offset + 3]) << 24);
    offset += 4;
    return value;
}

// 3. Ghi một chuỗi std::string (Format: [Length 4 bytes] + [Characters])
inline void writeString(std::vector<uint8_t>& buffer, const std::string& str) {
    // Ghi kích thước chuỗi trước
    writeUint32(buffer, static_cast<uint32_t>(str.length()));
    
    // Ghi từng ký tự
    for (char c : str) {
        buffer.push_back(static_cast<uint8_t>(c));
    }
}

// 4. Đọc chuỗi std::string từ buffer
inline std::string readString(const std::vector<uint8_t>& buffer, size_t& offset) {
    // Đọc độ dài chuỗi (offset sẽ tự tăng thêm 4)
    uint32_t length = readUint32(buffer, offset);
    
    if (offset + length > buffer.size()) {
        throw std::out_of_range("Buffer overflow khi đọc chuỗi");
    }
    
    // Cắt mảng byte ra thành chuỗi
    std::string str(buffer.begin() + offset, buffer.begin() + offset + length);
    offset += length;
    return str;
}

} // namespace utils
} // namespace graphlite