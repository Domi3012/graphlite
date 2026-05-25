/**
 * @file GraphDB.h
 * @brief Định nghĩa lớp Giao diện cốt lõi của GraphLite Database.
 * @version 0.1
 * @date 2026-05
 * * GraphLite là một hệ quản trị cơ sở dữ liệu đồ thị in-memory, 
 * sử dụng kiến trúc Danh sách kề (Adjacency List) siêu tốc trên RAM 
 * và đồng bộ dữ liệu xuống ổ đĩa bằng kiến trúc Log-structured (Bitcask).
 */

#pragma once
#include <string>
#include <cstdint>

// Nhúng các mảnh ghép nội bộ
#include "types.h"
#include "Schema.h"
#include "../../src/utils/MiniVector.h"
#include "../../src/utils/StringPoolDictionary.h"
#include "../../src/engine/BitcaskEngine.h"

namespace graphlite {

/**
 * @class GraphDB
 * @brief Trái tim của hệ thống GraphLite, quản lý vòng đời đồ thị và I/O.
 * * Lớp này cung cấp API để:
 * - Định nghĩa Schema (Loại đỉnh, Loại cạnh).
 * - Thao tác mạng lưới (Thêm đỉnh, Thêm cạnh mang Payload vô danh).
 * - Truy vấn hàng xóm O(1).
 * - Đồng bộ (Serialize) toàn bộ đồ thị trên RAM xuống ổ đĩa cứng.
 * * @warning Lớp này KHÔNG an toàn với đa luồng (Non-thread-safe). 
 * Cần có cơ chế Mutex/Lock ở tầng ứng dụng nếu sử dụng đa luồng.
 */
class GraphDB {
private:
    BitcaskEngine storage_;                                         ///< Engine quản lý I/O vật lý xuống file .db
    Schema schema_;                                                 ///< Quản lý siêu dữ liệu (Metadata) cho Đỉnh và Cạnh
    utils::StringPoolDictionary node_dict_;                         ///< Sổ từ điển ánh xạ Tên Đỉnh (String) -> NodeID (uint32_t)
    utils::MiniVector<utils::MiniVector<GenericEdge>> adjacency_list_; ///< Danh sách kề O(1) lưu trữ mạng lưới đồ thị

    /**
     * @brief Đảm bảo danh sách kề có đủ sức chứa cho một NodeID mới.
     * Cấp phát thêm các vector rỗng nếu ID vượt quá kích thước mảng hiện tại.
     * @param id NodeID cần đảm bảo sức chứa.
     */
    void ensureCapacityForNode(NodeID id);

public:
    /**
     * @brief Khởi tạo hệ quản trị Cơ sở dữ liệu Đồ thị.
     * Khởi động Bitcask Engine và cấp phát sẵn 50MB RAM cho StringPool.
     * * @param db_path Đường dẫn đến file cơ sở dữ liệu (ví dụ: "data.db").
     * Nếu file chưa tồn tại, hệ thống sẽ tự động tạo mới.
     */
    explicit GraphDB(const std::string& db_path);

    /**
     * @brief Hàm hủy của GraphDB.
     * Tự động giải phóng StringPool và đóng luồng Bitcask an toàn.
     */
    ~GraphDB();

    // ==========================================
    // CƠ CHẾ AN TOÀN (SAFETY CONSTRAINTS)
    // ==========================================
    
    /** @brief Xóa copy constructor để tránh nhân bản Database handle gây lỗi I/O và rò rỉ RAM. */
    GraphDB(const GraphDB&) = delete;
    
    /** @brief Xóa copy assignment operator. */
    GraphDB& operator=(const GraphDB&) = delete;


    // ==========================================
    // API TẦNG SCHEMA (CẤU HÌNH ĐỒ THỊ)
    // ==========================================

    /**
     * @brief Đăng ký một Loại Đỉnh (Node Type) mới vào hệ thống.
     * @param type_name Tên của loại đỉnh (Ví dụ: "User", "Device").
     * @return NodeType ID phân loại (8-bit) dùng để lưu trữ tối ưu trên RAM.
     */
    NodeType defineNodeType(const std::string& type_name);

    /**
     * @brief Đăng ký một Loại Cạnh (Edge Type) mới vào hệ thống.
     * @param type_name Tên của loại cạnh (Ví dụ: "USES", "ACCESSES").
     * @return EdgeType ID phân loại (8-bit) dùng để định danh mối quan hệ.
     */
    EdgeType defineEdgeType(const std::string& type_name);


    // ==========================================
    // API TẦNG ĐỒ THỊ (TƯƠNG TÁC MẠNG LƯỚI)
    // ==========================================

    /**
     * @brief Thêm một Đỉnh mới vào hệ thống (hoặc lấy ID nếu đã tồn tại).
     * @note Độ phức tạp: O(1) trung bình (Dựa trên Hash Map).
     * * @param node_name Tên định danh của Đỉnh (Ví dụ: "U001").
     * @return NodeID ID nguyên thủy (32-bit) đại diện cho đỉnh này.
     */
    NodeID addNode(const std::string& node_name);

    /**
     * @brief Tìm ID của một Đỉnh dựa vào tên định danh.
     * @param node_name Tên định danh của Đỉnh.
     * @return NodeID ID của đỉnh. Trả về 0 nếu đỉnh không tồn tại.
     */
    NodeID getNodeId(const std::string& node_name);

    /**
     * @brief Nối 2 đỉnh lại bằng một Cạnh mang gói dữ liệu nhị phân (Opaque Payload).
     * Áp dụng cơ chế Upsert: Tự động cấp phát bộ nhớ vật lý nếu đỉnh chưa từng tồn tại.
     * * @param from_id ID của Đỉnh xuất phát.
     * @param to_id ID của Đỉnh đích.
     * @param edge_type Loại mối quan hệ (Lấy từ hàm defineEdgeType).
     * @param payload Con trỏ trỏ đến struct dữ liệu tùy chỉnh của Application (Mặc định: nullptr).
     * @param payload_size Kích thước của struct tính bằng byte (Tối đa 11 bytes) (Mặc định: 0).
     * * @return true Nếu thêm cạnh thành công.
     */
    bool addEdge(NodeID from_id, NodeID to_id, EdgeType edge_type, 
                 const uint8_t* payload = nullptr, uint8_t payload_size = 0);

    /**
     * @brief Lấy toàn bộ danh sách Cạnh (hàng xóm) trỏ ra từ một Đỉnh.
     * @note Độ phức tạp: O(1) tuyệt đối nhờ truy xuất mảng trực tiếp.
     * * @param node_id ID của Đỉnh cần truy vấn.
     * @return const utils::MiniVector<GenericEdge>& Tham chiếu hằng số đến danh sách cạnh.
     * Trả về danh sách rỗng nếu NodeID không hợp lệ hoặc không có cạnh nào.
     */
    const utils::MiniVector<GenericEdge>& getEdges(NodeID node_id) const;


    // ==========================================
    // API TẦNG LƯU TRỮ (ĐỒNG BỘ I/O)
    // ==========================================

    /**
     * @brief Tuần tự hóa (Serialize) toàn bộ đồ thị trên RAM và ghi xuống Bitcask.
     * * Hàm này duyệt qua Danh sách kề, ép kiểu các mảng GenericEdge thành luồng byte thô
     * và ghi liên tục xuống ổ đĩa, sau đó gọi fsync để ép OS xả buffer.
     * * @note Chỉ nên gọi hàm này MỘT LẦN duy nhất sau khi Bulk Load (Nạp lượng lớn dữ liệu)
     * hoặc định kỳ để sao lưu, vì chi phí I/O ổ cứng rất cao.
     */
    void sync();
};

} // namespace graphlite