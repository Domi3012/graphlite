/**
 * @file traversal.h
 * @brief Định nghĩa cỗ máy duyệt đồ thị và giao diện Callback (Strategy Pattern).
 * @version 0.1
 * @date 2026-05
 * * * Module này tách biệt hoàn toàn thuật toán duyệt (Graph Traversal) 
 * khỏi logic nghiệp vụ của ứng dụng. Ứng dụng sẽ giao tiếp với lõi 
 * thông qua một bản hợp đồng (Interface) để quyết định đường đi.
 */

#pragma once
#include "GraphDB.h"
#include "../../src/utils/MiniVector.h"

namespace graphlite {

// ==================================================
// BẢN HỢP ĐỒNG CALLBACK (THE FILTER CONTRACT)
// ==================================================

/**
 * @class ITraversalCallback
 * @brief Giao diện trừu tượng (Interface) định nghĩa Chiến lược duyệt đồ thị.
 * * * Ứng dụng (Client) bắt buộc phải kế thừa lớp này và triển khai các hàm ảo 
 * để điều khiển hành vi của thuật toán duyệt (ví dụ: lọc theo thời gian, 
 * lọc theo loại sự kiện).
 */
class ITraversalCallback {
public:
    /** @brief Hàm hủy ảo mặc định để đảm bảo giải phóng bộ nhớ đúng cách ở lớp con. */
    virtual ~ITraversalCallback() = default;

    /**
     * @brief Hook kiểm duyệt: Được gọi MỖI KHI Engine định bước qua một Cạnh.
     * @param current_node ID của Đỉnh hiện tại đang đứng.
     * @param edge Tham chiếu hằng đến Cạnh (chứa Payload) sắp đi qua.
     * @return true Nếu ứng dụng cho phép đi qua cạnh này.
     * @return false Nếu ứng dụng muốn chặn (Cắt tỉa nhánh - Pruning) đường này.
     */
    virtual bool shouldTraverse(NodeID current_node, const GenericEdge& edge) = 0;
    
    /**
     * @brief Hook báo cáo: Được gọi MỖI KHI Engine đặt chân đến một Đỉnh hợp lệ.
     * @note Dùng để ứng dụng ghi log, đếm số bước nhảy, hoặc lưu lộ trình.
     * @param node ID của Đỉnh vừa đặt chân tới.
     */
    virtual void onNodeVisited(NodeID node) = 0;
};

// ==================================================
// CỖ MÁY DUYỆT ĐỒ THỊ (THE TRAVERSAL ENGINE)
// ==================================================

/**
 * @class TraversalEngine
 * @brief Cỗ máy thực thi các thuật toán trên đồ thị (Ví dụ: DFS).
 * * * Cỗ máy này chỉ giữ tham chiếu Chỉ đọc (Read-only) tới GraphDB, 
 * đảm bảo tuyệt đối không làm thay đổi cấu trúc dữ liệu trên RAM trong quá trình duyệt.
 */
class TraversalEngine {
private:
    /** @brief Tham chiếu hằng số tới Database chứa đồ thị. */
    const GraphDB& db_;

public:
    /**
     * @brief Khởi tạo cỗ máy duyệt đồ thị.
     * @param db Database mục tiêu cần duyệt.
     */
    explicit TraversalEngine(const GraphDB& db);

    /**
     * @brief Thuật toán Tìm kiếm theo chiều sâu (Depth-First Search - DFS).
     * @note Độ phức tạp thời gian: Tối đa O(V + E) trên lý thuyết, nhưng thực tế
     * sẽ nhanh hơn rất nhiều nhờ cơ chế cắt tỉa của tham số `max_depth` và `callback`.
     * Thuật toán có sử dụng Backtracking để tránh mắc kẹt trong vòng lặp (Cycle).
     * * @param start_node ID của Đỉnh bắt đầu hành trình.
     * @param max_depth Giới hạn độ sâu đệ quy (Chống tràn Stack và giới hạn bán kính tìm kiếm).
     * @param callback Đối tượng chứa logic lọc (Strategy) được chích (Inject) từ ứng dụng.
     */
    void dfs(NodeID start_node, int max_depth, ITraversalCallback& callback);

private:
    /**
     * @brief Hàm đệ quy nội bộ phục vụ cho DFS.
     * @param current_node Đỉnh đang đứng ở bước đệ quy hiện tại.
     * @param current_depth Độ sâu hiện tại (tính từ start_node là 0).
     * @param max_depth Giới hạn độ sâu tối đa cho phép.
     * @param callback Tham chiếu đến bộ lọc của ứng dụng.
     * @param visited Mảng đánh dấu các đỉnh đã đi qua để chống lặp vòng (Cycle Detection).
     */
    void dfsRecursive(NodeID current_node, int current_depth, int max_depth, 
                      ITraversalCallback& callback, utils::MiniVector<bool>& visited);
};

} // namespace graphlite