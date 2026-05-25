/**
 * @file Schema.h
 * @brief Định nghĩa lớp Schema (Siêu dữ liệu) cho GraphLite.
 * @version 0.1
 * @date 2026-05
 * * Lớp này đóng vai trò như một "Thông dịch viên" (Metadata Layer),
 * chịu trách nhiệm ánh xạ các chuỗi định danh ngôn ngữ tự nhiên (như "User", "USES")
 * thành các mã số nguyên 8-bit (NodeType, EdgeType) để lõi hệ thống
 * lưu trữ và xử lý một cách tối ưu trên RAM và ổ đĩa.
 */

#pragma once
#include <string>
#include <cstdint>
#include "../../src/utils/StringPoolDictionary.h"
#include "types.h"

namespace graphlite {

/**
 * @class Schema
 * @brief Quản lý siêu dữ liệu (Metadata) của Đồ thị, giới hạn ở 255 loại Đỉnh/Cạnh.
 * * Sử dụng StringPoolDictionary nội bộ được cấp phát sẵn kích thước siêu nhỏ
 * (1MB Pool, 256 slots) để tiết kiệm tối đa tài nguyên bộ nhớ, do số lượng
 * Loại Đỉnh và Loại Cạnh trong một đồ thị thực tế hiếm khi vượt quá vài chục.
 */
class Schema {
private:
    /** @brief Từ điển lưu trữ và cấp phát ID cho các phân loại Đỉnh (Node Types). */
    utils::StringPoolDictionary node_types_;
    
    /** @brief Từ điển lưu trữ và cấp phát ID cho các phân loại Cạnh (Edge Types). */
    utils::StringPoolDictionary edge_types_;

public:
    /**
     * @brief Khởi tạo hệ thống Schema với cấu hình bộ nhớ siêu nhẹ.
     * Cấp phát 2 StringPoolDictionary, mỗi cái chỉ dùng 1MB RAM và 256 slots.
     */
    Schema() 
        : node_types_(1, 256), 
          edge_types_(1, 256) {}

    /**
     * @brief Hàm hủy mặc định.
     * Lớp StringPoolDictionary nội bộ sẽ tự động dọn dẹp vùng nhớ Arena của nó.
     */
    ~Schema() = default;

    // ==========================================
    // CƠ CHẾ AN TOÀN (SAFETY CONSTRAINTS)
    // ==========================================

    /** @brief Vô hiệu hóa Copy Constructor để tránh nhân bản từ điển, gây rò rỉ RAM hoặc ID không đồng bộ. */
    Schema(const Schema&) = delete;
    
    /** @brief Vô hiệu hóa Copy Assignment Operator. */
    Schema& operator=(const Schema&) = delete;

    // ==========================================
    // QUẢN LÝ NODE TYPE (LOẠI ĐỈNH)
    // ==========================================

    /**
     * @brief Đăng ký một Loại Đỉnh mới vào hệ thống.
     * Nếu tên loại đỉnh đã tồn tại, hàm sẽ trả về ID cũ.
     * * @warning Hệ thống ép kiểu (downcast) từ uint32_t xuống uint8_t.
     * Do đó, người dùng thư viện KHÔNG được đăng ký quá 255 loại đỉnh khác nhau.
     * * @param type_name Tên định danh của loại đỉnh (Ví dụ: "User", "Device").
     * @return NodeType ID phân loại (8-bit).
     */
    NodeType registerNodeType(const std::string& type_name) {
        uint32_t id = node_types_.get_or_create_id(type_name);
        return static_cast<NodeType>(id);
    }

    /**
     * @brief Lấy ID của một Loại Đỉnh đã tồn tại.
     * @note Nếu loại đỉnh này chưa từng tồn tại, StringPool sẽ tự động tạo ID mới.
     * * @param type_name Tên định danh của loại đỉnh.
     * @return NodeType ID phân loại (8-bit).
     */
    NodeType getNodeTypeId(const std::string& type_name) {
        return static_cast<NodeType>(node_types_.get_or_create_id(type_name));
    }

    // ==========================================
    // QUẢN LÝ EDGE TYPE (LOẠI CẠNH)
    // ==========================================

    /**
     * @brief Đăng ký một Loại Cạnh mới vào hệ thống.
     * Tương tự như Đỉnh, hệ thống giới hạn tối đa 255 loại cạnh.
     * * @param type_name Tên định danh của loại cạnh (Ví dụ: "USES", "LOCATED_IN").
     * @return EdgeType ID phân loại (8-bit).
     */
    EdgeType registerEdgeType(const std::string& type_name) {
        uint32_t id = edge_types_.get_or_create_id(type_name);
        return static_cast<EdgeType>(id);
    }

    /**
     * @brief Lấy ID của một Loại Cạnh đã tồn tại.
     * * @param type_name Tên định danh của loại cạnh.
     * @return EdgeType ID phân loại (8-bit).
     */
    EdgeType getEdgeTypeId(const std::string& type_name) {
        return static_cast<EdgeType>(edge_types_.get_or_create_id(type_name));
    }
};

} // namespace graphlite