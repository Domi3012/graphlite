#include <graphlite/traversal.h>
#include <graphlite/internal/MiniQueue.h>
#include <utility>

namespace graphlite {

TraversalEngine::TraversalEngine(const GraphDB& db) : db_(db) {}

void TraversalEngine::dfs(NodeID start_node, int max_depth, ITraversalCallback& callback) {
    size_t node_count = db_.getNodeCount();
    // Safety check in case graph is completely empty
    if (start_node == 0 || node_count == 0) return;
    
    // node_count + 1 because IDs start at 1
    utils::MiniVector<bool> visited(node_count + 1, false);
    
    // Stack lưu trữ <NodeID, current_depth>
    utils::MiniVector<std::pair<NodeID, int>> stack;
    stack.push_back({start_node, 0});

    while (!stack.empty()) {
        auto [current_node, current_depth] = stack.back();
        stack.pop_back(); // Remove top

        if (current_depth > max_depth) continue;

        if (current_node >= visited.size() || visited[current_node]) {
            continue;
        }

        visited[current_node] = true;
        callback.onNodeVisited(current_node);

        if (current_depth < max_depth) {
            const auto& edges = db_.getOutEdges(current_node);
            // Để duyệt theo thứ tự giống đệ quy (chiều sâu xuống nhánh đầu tiên), 
            // ta push các con vào stack theo thứ tự NGƯỢC LẠI
            for (size_t i = edges.size(); i > 0; --i) {
                const GenericEdge& edge = edges[i - 1];
                if (callback.shouldTraverse(current_node, edge)) {
                    if (edge.target_node < visited.size() && !visited[edge.target_node]) {
                        stack.push_back({edge.target_node, current_depth + 1});
                    }
                }
            }
        }
    }
}

void TraversalEngine::bfs(NodeID start_node, int max_depth, ITraversalCallback& callback) {
    size_t node_count = db_.getNodeCount();
    if (start_node == 0 || node_count == 0) return;

    utils::MiniVector<bool> visited(node_count + 1, false);
    
    // Queue lưu trữ <NodeID, current_depth>
    utils::MiniQueue<std::pair<NodeID, int>> queue;
    queue.push({start_node, 0});
    
    // Đánh dấu visited ngay khi đưa vào queue (chuẩn BFS)
    if (start_node < visited.size()) {
        visited[start_node] = true;
    }

    while (!queue.empty()) {
        auto [current_node, current_depth] = queue.front();
        queue.pop();

        callback.onNodeVisited(current_node);

        if (current_depth < max_depth) {
            const auto& edges = db_.getOutEdges(current_node);
            for (size_t i = 0; i < edges.size(); ++i) {
                const GenericEdge& edge = edges[i];
                if (callback.shouldTraverse(current_node, edge)) {
                    if (edge.target_node < visited.size() && !visited[edge.target_node]) {
                        visited[edge.target_node] = true;
                        queue.push({edge.target_node, current_depth + 1});
                    }
                }
            }
        }
    }
}

// ============================================================
// UTILITY FUNCTIONS
// ============================================================

utils::MiniVector<NodeID> MatchPathPattern(const GraphDB& db, NodeID start_node, const utils::MiniVector<EdgeType>& pattern) {
    utils::MiniVector<NodeID> results;
    if (pattern.empty() || start_node == 0) return results;

    // Stack lưu <NodeID, pattern_index>
    utils::MiniVector<std::pair<NodeID, size_t>> stack;
    stack.push_back({start_node, 0});

    while (!stack.empty()) {
        auto [current_node, pattern_idx] = stack.back();
        stack.pop_back();

        // Nếu đã khớp hết pattern
        if (pattern_idx == pattern.size()) {
            // Tránh duplicate
            bool exists = false;
            for (size_t i = 0; i < results.size(); ++i) {
                if (results[i] == current_node) { exists = true; break; }
            }
            if (!exists) results.push_back(current_node);
            continue;
        }

        EdgeType required_type = pattern[pattern_idx];
        const auto& edges = db.getOutEdges(current_node);
        
        for (size_t i = edges.size(); i > 0; --i) {
            const GenericEdge& edge = edges[i - 1];
            if (edge.edge_type == required_type) {
                stack.push_back({edge.target_node, pattern_idx + 1});
            }
        }
    }

    return results;
}

size_t CountNeighborsIf(const GraphDB& db, NodeID node, EdgeType type, bool is_out_edge) {
    size_t count = 0;
    if (is_out_edge) {
        const auto& edges = db.getOutEdges(node);
        for (size_t i = 0; i < edges.size(); ++i) {
            if (edges[i].edge_type == type) count++;
        }
    } else {
        const auto& edges = db.getInEdges(node);
        for (size_t i = 0; i < edges.size(); ++i) {
            if (edges[i].edge_type == type) count++;
        }
    }
    return count;
}

} // namespace graphlite
