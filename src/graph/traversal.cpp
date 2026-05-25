#include "../../include/graphlite/traversal.h"

namespace graphlite {

TraversalEngine::TraversalEngine(const GraphDB& db) : db_(db) {}

void TraversalEngine::dfs(NodeID start_node, int max_depth, ITraversalCallback& callback) {
    // Để cấp phát mảng visited, ta cần biết đỉnh có ID lớn nhất trong đồ thị.
    // Tuy nhiên, để đơn giản và siêu nhanh cho đồ án, ta cấp phát 1 triệu phần tử ban đầu (chỉ tốn 1MB RAM)
    utils::MiniVector<bool> visited;
    
    // Mẹo C++: Push sẵn giá trị false vào mảng
    for (size_t i = 0; i < 2000000; ++i) { 
        visited.push_back(false);
    }

    // Bắt đầu hành trình
    dfsRecursive(start_node, 0, max_depth, callback, visited);
}

void TraversalEngine::dfsRecursive(NodeID current_node, int current_depth, int max_depth, 
                                   ITraversalCallback& callback, utils::MiniVector<bool>& visited) {
    
    // 1. Điểm dừng: Quá độ sâu cho phép
    if (current_depth >= max_depth) return;

    // 2. Điểm dừng: Nếu ID nằm ngoài giới hạn mảng visited hoặc đã đi qua
    if (current_node >= visited.size() || visited[current_node]) return;

    // 3. Đánh dấu đã đặt chân đến
    visited[current_node] = true;
    callback.onNodeVisited(current_node); // Báo cáo cho Application

    // 4. Lấy danh sách hàng xóm của đỉnh hiện tại
    const auto& edges = db_.getEdges(current_node);

    // 5. Duyệt qua từng Cạnh
    for (size_t i = 0; i < edges.size(); ++i) {
        const GenericEdge& edge = edges[i];

        // 6. HỎI Ý KIẾN APPLICATION: Có cho phép đi qua cái Cạnh này không?
        if (callback.shouldTraverse(current_node, edge)) {
            // Nhảy sang đỉnh đích (Đệ quy)
            dfsRecursive(edge.target_node, current_depth + 1, max_depth, callback, visited);
        }
    }

    // 7. Quay lui (Backtracking) - Xóa vết để các đường đi khác có thể thăm lại đỉnh này
    visited[current_node] = false;
}

} // namespace graphlite