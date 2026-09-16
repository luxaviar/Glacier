#include "Animation/NodeLookup.h"
#include "Core/Transform.h"
#include "Core/GameObject.h"

namespace glacier {

void CollectNodeTransforms(const Transform& node, std::unordered_map<std::string, Transform*>& out) {
    out.emplace(node.game_object()->name(), const_cast<Transform*>(&node));

    for (auto* child : node.children()) {
        CollectNodeTransforms(*child, out);
    }
}

Transform* FindNodeTransform(const Transform& any_node, const char* name) {
    if (!name) return nullptr;

    auto* root = &any_node;
    while (root->parent()) {
        root = root->parent();
    }

    std::unordered_map<std::string, Transform*> nodes;
    CollectNodeTransforms(*root, nodes);

    auto it = nodes.find(name);
    return it != nodes.end() ? it->second : nullptr;
}

}
