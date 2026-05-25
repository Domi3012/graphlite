/**
 * @file types.h
 * @brief Định nghĩa các kiểu dữ liệu cốt lõi và cấu trúc Cạnh (Edge) cho GraphLite.
 * @version 0.1
 * @date 2026-05
 * * Các cấu trúc trong file này được thiết kế theo chuẩn POD (Plain Old Data) 
 * để tối ưu hóa việc tuần tự hóa (Serialization) và thân thiện với CPU Cache.
 */

#pragma once
#include <cstdint>
#include <cstring>   // Cho std::memset, std::memcpy
#include <algorithm> // Cho std::min

namespace graphlite {

// ==================================================
// ĐỊNH DANH HỆ THỐNG (SYSTEM IDENTIFIERS)
// ==================================================

/** * @typedef NodeID
 * @brief Định danh nguyên thủy của Đỉnh.
 * Sử dụng số nguyên không dấu 32-bit, cung cấp không gian định danh cho ~4.2 tỷ đỉnh.
 */
using NodeID = uint32_t;

/** * @typedef NodeType
 * @brief Định danh phân loại Đỉnh (Ví dụ: User, Device).
 * Sử dụng 8-bit để tiết kiệm RAM, giới hạn tối đa 255 loại đỉnh khác nhau.
 */
using NodeType = uint8_t;

/** * @typedef EdgeType
 * @brief Định danh phân loại Cạnh (Ví dụ: USES, CONNECTS).
 * Giới hạn tối đa 255 loại cạnh khác nhau.
 */
using EdgeType = uint8_t;

// ==================================================
// CẤU TRÚC VẬT LÝ CỦA MẠNG LƯỚI (GRAPH STRUCTURES)
// ==================================================

/** * @brief Kích thước tối đa của gói hàng mờ (Opaque Payload).
 * @note Con số 11 được tính toán cực kỳ có chủ đích: 
 * 4 bytes (NodeID) + 1 byte (EdgeType) + 11 bytes (Payload) = ĐÚNG 16 BYTES.
 * Kích thước lũy thừa của 2 này giúp CPU đẩy mảng GenericEdge vào L1/L2 Cache 
 * với tốc độ vật lý tối đa.
 */
constexpr uint8_t MAX_PAYLOAD_SIZE = 11;

/**
 * @struct GenericEdge
 * @brief Cấu trúc biểu diễn một Cạnh có hướng, mang dữ liệu tùy biến.
 * * * GenericEdge không trực tiếp định nghĩa thuộc tính (như timestamp, weight). 
 * Thay vào đó, nó mang một mảng byte vô danh (`payload`). Tầng Application 
 * (ví dụ: HALO) sẽ tự ép kiểu (reinterpret_cast) mảng byte này thành struct 
 * nghiệp vụ của riêng họ.
 */
struct GenericEdge {
    NodeID target_node;                         ///< ID của đỉnh đích mà cạnh này trỏ tới.
    EdgeType edge_type;                         ///< Mã phân loại mối quan hệ.
    
    uint8_t payload[MAX_PAYLOAD_SIZE];          ///< Vùng nhớ đệm (buffer) chứa dữ liệu tùy chỉnh.

    /** * @brief Constructor mặc định. 
     * Bắt buộc phải có để các cấu trúc dạng mảng (như MiniVector) có thể cấp phát vùng nhớ.
     */
    GenericEdge() = default;

    /**
     * @brief Constructor khởi tạo Cạnh an toàn.
     * Tự động sao chép dữ liệu từ Application vào Payload và ngăn chặn lỗi tràn bộ đệm.
     * * @param target ID của đỉnh đích.
     * @param type Loại cạnh.
     * @param raw_payload Con trỏ trỏ tới struct dữ liệu của Application (Ví dụ: &my_struct).
     * @param payload_size Kích thước thực tế của struct (dùng sizeof).
     */
    GenericEdge(NodeID target, EdgeType type, const uint8_t* raw_payload, uint8_t payload_size)
        : target_node(target), edge_type(type) {
        
        // 1. Zero-out: Xóa sạch rác bộ nhớ cũ để tránh rò rỉ dữ liệu nhạy cảm
        // và đảm bảo tính nhất quán khi ghi xuống ổ đĩa (Bitcask).
        std::memset(payload, 0, MAX_PAYLOAD_SIZE);
        
        // 2. Sao chép an toàn (Safe-copy): Chỉ copy tối đa MAX_PAYLOAD_SIZE bytes
        // để phòng chống lỗi Buffer Overflow nếu Application truyền size quá lớn.
        if (raw_payload != nullptr && payload_size > 0) {
            uint8_t copy_size = std::min(payload_size, MAX_PAYLOAD_SIZE);
            std::memcpy(payload, raw_payload, copy_size);
        }
    }
};

} // namespace graphlite