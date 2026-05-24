#pragma once
#include <cstdint>
#include <cstring>   // Cho std::memset, std::memcpy
#include <algorithm> // Cho std::min

namespace graphlite {

// Định danh Đỉnh (Node) - 32-bit (Sức chứa: ~4.2 tỷ đỉnh)
using NodeID = uint32_t;

// Phân loại Đỉnh và Cạnh - 8-bit (Sức chứa: 255 loại)
using NodeType = uint8_t;
using EdgeType = uint8_t;

// Kích thước Payload cố định để tổng GenericEdge vừa khít 16 bytes
constexpr uint8_t MAX_PAYLOAD_SIZE = 11;

// Cấu trúc vật lý của một Cạnh mang thuộc tính vô danh
struct GenericEdge {
    NodeID target_node;
    EdgeType edge_type;
    
    // Gói hàng mờ (Opaque Payload) chở dữ liệu cho Application
    uint8_t payload[MAX_PAYLOAD_SIZE];

    // Constructor mặc định (cần thiết cho việc khởi tạo mảng)
    GenericEdge() = default;

    // Định nghĩa trực tiếp (Inline implementation) để compiler tối ưu tốc độ
    GenericEdge(NodeID target, EdgeType type, const uint8_t* raw_payload, uint8_t payload_size)
        : target_node(target), edge_type(type) {
        
        // 1. Zero-out memory tránh rác RAM
        std::memset(payload, 0, MAX_PAYLOAD_SIZE);
        
        // 2. Chép dữ liệu từ Application vào (có kiểm tra chống tràn bộ đệm)
        if (raw_payload != nullptr && payload_size > 0) {
            uint8_t copy_size = std::min(payload_size, MAX_PAYLOAD_SIZE);
            std::memcpy(payload, raw_payload, copy_size);
        }
    }
};

} // namespace graphlite