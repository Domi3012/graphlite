/**
 * @file traversal.h
 * @brief Cỗ máy duyệt đồ thị — DFS, BFS, Strategy Pattern callback.
 * @version 1.0 (STUB — sẽ hoàn thiện ở Phase 4)
 */

#pragma once
#include "GraphDB.h"
#include "internal/MiniVector.h"

namespace graphlite {

// ============================================================
// CALLBACK INTERFACE (Strategy Pattern)
// ============================================================

/**
 * @class ITraversalCallback
 * @brief Giao diện trừu tượng — ứng dụng implement để điều khiển duyệt đồ thị.
 */
class ITraversalCallback {
public:
    virtual ~ITraversalCallback() = default;

    /**
     * @brief Hook kiểm duyệt: Có cho phép đi qua cạnh này?
     * @param current_node ID đỉnh hiện tại.
     * @param edge Cạnh sắp đi qua.
     * @return true nếu cho phép, false nếu cắt tỉa nhánh.
     */
    virtual bool shouldTraverse(NodeID current_node, const GenericEdge& edge) = 0;

    /**
     * @brief Hook báo cáo: Được gọi khi Engine đặt chân đến một đỉnh.
     * @param node ID đỉnh vừa đến.
     */
    virtual void onNodeVisited(NodeID node) = 0;
};

// ============================================================
// TRAVERSAL ENGINE
// ============================================================

/**
 * @class TraversalEngine
 * @brief Cỗ máy thực thi DFS/BFS trên đồ thị.
 */
class TraversalEngine {
private:
    const GraphDB& db_;

    void dfsRecursive(NodeID current_node, int current_depth, int max_depth,
                      ITraversalCallback& callback, utils::MiniVector<bool>& visited);

public:
    explicit TraversalEngine(const GraphDB& db);

    /**
     * @brief Depth-First Search.
     * @param start_node Đỉnh bắt đầu.
     * @param max_depth Giới hạn độ sâu.
     * @param callback Chiến lược lọc.
     */
    void dfs(NodeID start_node, int max_depth, ITraversalCallback& callback);

    // TODO Phase 4: BFS, iterative DFS
};

// ============================================================
// IMPLEMENTATION
// ============================================================

#ifdef GRAPHLITE_IMPL_GUARD

GRAPHLITE_FUNC TraversalEngine::TraversalEngine(const GraphDB& db) : db_(db) {}

GRAPHLITE_FUNC void TraversalEngine::dfs(NodeID start_node, int max_depth,
                                          ITraversalCallback& callback) {
    // Dynamic sizing thay vì hardcode 2M phần tử
    size_t visited_size = 100000; // Sẽ được thay bằng node_store_.nodeCount() ở Phase 4
    utils::MiniVector<bool> visited(visited_size, false);
    dfsRecursive(start_node, 0, max_depth, callback, visited);
}

GRAPHLITE_FUNC void TraversalEngine::dfsRecursive(
    NodeID current_node, int current_depth, int max_depth,
    ITraversalCallback& callback, utils::MiniVector<bool>& visited) {

    if (current_depth >= max_depth) return;
    if (current_node >= visited.size() || visited[current_node]) return;

    visited[current_node] = true;
    callback.onNodeVisited(current_node);

    const auto& edges = db_.getEdges(current_node);
    for (size_t i = 0; i < edges.size(); ++i) {
        const GenericEdge& edge = edges[i];
        if (callback.shouldTraverse(current_node, edge)) {
            dfsRecursive(edge.target_node, current_depth + 1, max_depth, callback, visited);
        }
    }

    visited[current_node] = false;  // Backtracking
}

#endif // GRAPHLITE_IMPL_GUARD

} // namespace graphlite