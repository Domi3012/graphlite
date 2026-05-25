#pragma once
#include "GraphDB.h"
#include "../../src/utils/MiniVector.h"

namespace graphlite {

// ==================================================
// BẢN HỢP ĐỒNG CALLBACK (The Filter Contract)
// ==================================================
class ITraversalCallback {
public:
    virtual ~ITraversalCallback() = default;

    // GraphLite sẽ gọi hàm này mỗi khi nó đứng ở 1 Đỉnh và định nhảy sang 1 Cạnh.
    // Nếu ứng dụng (HALO) trả về TRUE -> GraphLite đi tiếp.
    // Nếu ứng dụng trả về FALSE -> GraphLite chặn đường này lại (Cắt tỉa nhánh).
    virtual bool shouldTraverse(NodeID current_node, const GenericEdge& edge) = 0;
    
    // GraphLite gọi hàm này khi nó đã đặt chân đến Đỉnh đích thành công.
    // Dùng để HALO ghi log, in đường đi, hoặc đếm số lượng.
    virtual void onNodeVisited(NodeID node) = 0;
};

// ==================================================
// CỖ MÁY DUYỆT ĐỒ THỊ (The Engine)
// ==================================================
class TraversalEngine {
private:
    const GraphDB& db_; // Chỉ đọc (Read-only reference)

public:
    explicit TraversalEngine(const GraphDB& db);

    // Thuật toán Tìm kiếm theo chiều sâu (DFS)
    // - start_node: Điểm xuất phát
    // - max_depth: Giới hạn độ sâu (Chống tràn Stack và lặp vô hạn)
    // - callback: Đối tượng chứa logic của HALO
    void dfs(NodeID start_node, int max_depth, ITraversalCallback& callback);

private:
    // Hàm đệ quy nội bộ
    void dfsRecursive(NodeID current_node, int current_depth, int max_depth, 
                      ITraversalCallback& callback, utils::MiniVector<bool>& visited);
};

} // namespace graphlite