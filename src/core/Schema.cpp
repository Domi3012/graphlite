#include <graphlite/Schema.h>
#include "../utils/StringPoolDictionary.h"

namespace graphlite {

struct Schema::Impl {
    utils::StringPoolDictionary node_types_;
    utils::StringPoolDictionary edge_types_;

    Impl() : node_types_(1, 256), edge_types_(1, 256) {}
};

Schema::Schema() : pimpl_(std::make_unique<Impl>()) {}

Schema::~Schema() = default;

Schema::Schema(Schema&&) noexcept = default;
Schema& Schema::operator=(Schema&&) noexcept = default;

NodeType Schema::registerNodeType(const std::string& type_name) {
    uint32_t id = pimpl_->node_types_.get_or_create_id(type_name);
    return static_cast<NodeType>(id);
}

NodeType Schema::getNodeTypeId(const std::string& type_name) {
    return static_cast<NodeType>(pimpl_->node_types_.get_or_create_id(type_name));
}

EdgeType Schema::registerEdgeType(const std::string& type_name) {
    uint32_t id = pimpl_->edge_types_.get_or_create_id(type_name);
    return static_cast<EdgeType>(id);
}

EdgeType Schema::getEdgeTypeId(const std::string& type_name) {
    return static_cast<EdgeType>(pimpl_->edge_types_.get_or_create_id(type_name));
}

} // namespace graphlite
