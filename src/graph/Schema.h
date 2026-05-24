#pragma once
#include <string>
#include <cstdint>
#include "../utils/StringPoolDictionary.h"
#include "types.h"

namespace graphlite {

class Schema {
private:
    // Sổ tay từ điển chuyên dùng để dịch Label của Đỉnh
    utils::StringPoolDictionary node_types_;
    
    // Sổ tay từ điển chuyên dùng để dịch Type của Cạnh
    utils::StringPoolDictionary edge_types_;

public:
    // Constructor: Cấp phát StringPool cực nhỏ (1MB pool, 256 slots)
    // Vì số lượng Type của một đồ thị hiếm khi vượt qua con số 20.
    Schema() 
        : node_types_(1, 256), 
          edge_types_(1, 256) {}

    ~Schema() = default;

    // Chặn Copy để tránh rò rỉ hoặc nhân bản bộ nhớ từ điển
    Schema(const Schema&) = delete;
    Schema& operator=(const Schema&) = delete;

    // ==========================================
    // QUẢN LÝ NODE TYPE (Ví dụ: "User", "Device")
    // ==========================================
    
    NodeType registerNodeType(const std::string& type_name) {
        // StringPool trả về uint32_t, ta ép thẳng về uint8_t (vì type max là 255)
        uint32_t id = node_types_.get_or_create_id(type_name);
        return static_cast<NodeType>(id);
    }

    NodeType getNodeTypeId(const std::string& type_name) {
        return static_cast<NodeType>(node_types_.get_or_create_id(type_name));
    }

    // ==========================================
    // QUẢN LÝ EDGE TYPE (Ví dụ: "USES", "LOCATED_IN")
    // ==========================================

    EdgeType registerEdgeType(const std::string& type_name) {
        uint32_t id = edge_types_.get_or_create_id(type_name);
        return static_cast<EdgeType>(id);
    }

    EdgeType getEdgeTypeId(const std::string& type_name) {
        return static_cast<EdgeType>(edge_types_.get_or_create_id(type_name));
    }
};

} // namespace graphlite