#include <graphlite/traversal.h>

namespace graphlite {

TraversalEngine::TraversalEngine(const GraphDB& db) : db_(db) {}

void TraversalEngine::dfs(NodeID start_node, int max_depth, ITraversalCallback& callback) {
    // Dynamic sizing thay vì hardcode 2M phần tử
    size_t visited_size = 100000; // Sẽ được thay bằng node_store_.nodeCount() ở Phase 4
    utils::MiniVector<bool> visited(visited_size, false);
    dfsRecursive(start_node, 0, max_depth, callback, visited);
}

void TraversalEngine::dfsRecursive(
    NodeID current_node, int current_depth, int max_depth,
    ITraversalCallback& callback, utils::MiniVector<bool>& visited) {

    if (current_depth >= max_depth) return;
    if (current_node >= visited.size() || visited[current_node]) return;

    visited[current_node] = true;
    callback.onNodeVisited(current_node);

    const auto& edges = db_.getOutEdges(current_node);
    for (size_t i = 0; i < edges.size(); ++i) {
        const GenericEdge& edge = edges[i];
        if (callback.shouldTraverse(current_node, edge)) {
            dfsRecursive(edge.target_node, current_depth + 1, max_depth, callback, visited);
        }
    }

    visited[current_node] = false;  // Backtracking
}

} // namespace graphlite
