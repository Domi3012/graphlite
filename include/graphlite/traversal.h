/**
 * @file traversal.h
 * @brief Cỗ máy duyệt đồ thị — DFS, BFS, Strategy Pattern callback.
 * @version 1.0 (STUB — sẽ hoàn thiện ở Phase 4)
 */

#pragma once
#include "GraphDB.h"
#include "MiniVector.h"

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

public:
    explicit TraversalEngine(const GraphDB& db);

    /**
     * @brief Depth-First Search (Iterative).
     * @param start_node Đỉnh bắt đầu.
     * @param max_depth Giới hạn độ sâu.
     * @param callback Chiến lược lọc.
     */
    void dfs(NodeID start_node, int max_depth, ITraversalCallback& callback);

    /**
     * @brief Breadth-First Search.
     * @param start_node Đỉnh bắt đầu.
     * @param max_depth Giới hạn độ sâu.
     * @param callback Chiến lược lọc.
     */
    void bfs(NodeID start_node, int max_depth, ITraversalCallback& callback);
};

// ============================================================
// UTILITY FUNCTIONS
// ============================================================

/**
 * @brief Tìm các đỉnh đích thoả mãn chuỗi kiểu cạnh chỉ định.
 * 
 * @param db Database.
 * @param start_node Đỉnh xuất phát.
 * @param pattern Mảng các kiểu cạnh liên tiếp.
 * @return Danh sách các NodeID thoả mãn chuỗi pattern.
 */
utils::MiniVector<NodeID> MatchPathPattern(const GraphDB& db, NodeID start_node, const utils::MiniVector<EdgeType>& pattern);

/**
 * @brief Đếm số láng giềng thoả mãn điều kiện.
 * 
 * @param db Database.
 * @param node Đỉnh cần xét.
 * @param type Kiểu cạnh để xét.
 * @param is_out_edge Nếu true đếm Out-edges, false đếm In-edges.
 * @return Số lượng láng giềng.
 */
size_t CountNeighborsIf(const GraphDB& db, NodeID node, EdgeType type, bool is_out_edge = true);

// ============================================================
// EDGE SORTING
// ============================================================

/**
 * @class IEdgeComparator
 * @brief Interface so sánh để sắp xếp cạnh.
 */
class IEdgeComparator {
public:
    virtual ~IEdgeComparator() = default;
    
    /**
     * @return true nếu a đứng trước b
     */
    virtual bool lessThan(const GenericEdge& a, const GenericEdge& b) const = 0;
};

/**
 * @brief Sắp xếp danh sách cạnh tại chỗ bằng thuật toán QuickSort.
 * 
 * @param edges Danh sách cạnh cần sắp xếp (ví dụ: trả về từ getOutEdges)
 * @param cmp Bộ so sánh
 */
void sortEdges(utils::MiniVector<GenericEdge>& edges, const IEdgeComparator& cmp);

} // namespace graphlite