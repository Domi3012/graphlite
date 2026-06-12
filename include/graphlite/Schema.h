/**
 * @file Schema.h
 * @brief Định nghĩa lớp Schema (Siêu dữ liệu) cho GraphLite.
 * @version 0.1
 * @date 2026-05
 */

#pragma once
#include <string>
#include <cstdint>
#include <memory>
#include "types.h"

namespace graphlite {

/**
 * @class Schema
 * @brief Quản lý siêu dữ liệu (Metadata) của Đồ thị, giới hạn ở 255 loại Đỉnh/Cạnh.
 */
class Schema {
private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;

public:
    /**
     * @brief Khởi tạo hệ thống Schema với cấu hình bộ nhớ siêu nhẹ.
     */
    Schema();

    /**
     * @brief Hàm hủy mặc định.
     */
    ~Schema();

    // ==========================================
    // CƠ CHẾ AN TOÀN (SAFETY CONSTRAINTS)
    // ==========================================

    Schema(const Schema&) = delete;
    Schema& operator=(const Schema&) = delete;
    Schema(Schema&&) noexcept;
    Schema& operator=(Schema&&) noexcept;

    // ==========================================
    // QUẢN LÝ NODE TYPE (LOẠI ĐỈNH)
    // ==========================================

    NodeType registerNodeType(const std::string& type_name);
    NodeType getNodeTypeId(const std::string& type_name);

    // ==========================================
    // QUẢN LÝ EDGE TYPE (LOẠI CẠNH)
    // ==========================================

    EdgeType registerEdgeType(const std::string& type_name);
    EdgeType getEdgeTypeId(const std::string& type_name);
};

} // namespace graphlite