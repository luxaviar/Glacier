#pragma once

#include <string>
#include <unordered_map>

namespace glacier {

class Transform;

//maps the name of every GameObject in the hierarchy containing `node` (node
//itself included) to its transform
void CollectNodeTransforms(const Transform& node, std::unordered_map<std::string, Transform*>& out);

//resolves a node by name, starting the search at the hierarchy root of `any_node`
Transform* FindNodeTransform(const Transform& any_node, const char* name);

}
