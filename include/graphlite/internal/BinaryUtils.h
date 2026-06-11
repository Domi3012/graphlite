/**
 * @file BinaryUtils.h
 * @brief Tiện ích đọc/ghi nhị phân — hỗ trợ cả std::vector và raw pointer API.
 * @version 1.0
 *
 * Tất cả hàm đều inline (nhỏ, performance-critical) — không cần hybrid macro.
 * Sử dụng Little-Endian byte order.
 */

#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <string>
#include <stdexcept>

namespace graphlite {
namespace utils {

// ============================================================
// RAW POINTER API (zero-copy, dùng cho mmap operations)
// ============================================================

/** @brief Ghi uint32_t vào buffer tại offset, tự tăng offset. */
inline void writeUint32(uint8_t* buf, size_t& offset, uint32_t value) {
    buf[offset]     = static_cast<uint8_t>(value & 0xFF);
    buf[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buf[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buf[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
    offset += 4;
}

/** @brief Đọc uint32_t từ buffer tại offset, tự tăng offset. */
inline uint32_t readUint32(const uint8_t* buf, size_t& offset) {
    uint32_t value = static_cast<uint32_t>(buf[offset])
                   | (static_cast<uint32_t>(buf[offset + 1]) << 8)
                   | (static_cast<uint32_t>(buf[offset + 2]) << 16)
                   | (static_cast<uint32_t>(buf[offset + 3]) << 24);
    offset += 4;
    return value;
}

/** @brief Ghi uint64_t vào buffer tại offset, tự tăng offset. */
inline void writeUint64(uint8_t* buf, size_t& offset, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        buf[offset + i] = static_cast<uint8_t>((value >> (i * 8)) & 0xFF);
    }
    offset += 8;
}

/** @brief Đọc uint64_t từ buffer tại offset, tự tăng offset. */
inline uint64_t readUint64(const uint8_t* buf, size_t& offset) {
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<uint64_t>(buf[offset + i]) << (i * 8);
    }
    offset += 8;
    return value;
}

/** @brief Ghi uint16_t vào buffer tại offset, tự tăng offset. */
inline void writeUint16(uint8_t* buf, size_t& offset, uint16_t value) {
    buf[offset]     = static_cast<uint8_t>(value & 0xFF);
    buf[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    offset += 2;
}

/** @brief Đọc uint16_t từ buffer tại offset, tự tăng offset. */
inline uint16_t readUint16(const uint8_t* buf, size_t& offset) {
    uint16_t value = static_cast<uint16_t>(buf[offset])
                   | (static_cast<uint16_t>(buf[offset + 1]) << 8);
    offset += 2;
    return value;
}

// ============================================================
// VECTOR API (backward-compatible với v0.1)
// ============================================================

inline void writeUint32(std::vector<uint8_t>& buffer, uint32_t value) {
    buffer.push_back(static_cast<uint8_t>(value & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buffer.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

inline uint32_t readUint32(const std::vector<uint8_t>& buffer, size_t& offset) {
    if (offset + 4 > buffer.size()) {
        throw std::out_of_range("Buffer overflow when reading uint32_t");
    }
    uint32_t value = static_cast<uint32_t>(buffer[offset])
                   | (static_cast<uint32_t>(buffer[offset + 1]) << 8)
                   | (static_cast<uint32_t>(buffer[offset + 2]) << 16)
                   | (static_cast<uint32_t>(buffer[offset + 3]) << 24);
    offset += 4;
    return value;
}

inline void writeString(std::vector<uint8_t>& buffer, const std::string& str) {
    writeUint32(buffer, static_cast<uint32_t>(str.length()));
    for (char c : str) {
        buffer.push_back(static_cast<uint8_t>(c));
    }
}

inline std::string readString(const std::vector<uint8_t>& buffer, size_t& offset) {
    uint32_t length = readUint32(buffer, offset);
    if (offset + length > buffer.size()) {
        throw std::out_of_range("Buffer overflow when reading string");
    }
    std::string str(buffer.begin() + offset, buffer.begin() + offset + length);
    offset += length;
    return str;
}

} // namespace utils
} // namespace graphlite
