#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace glacier {

class Transform;

//Transforms of every node of one generated instance, in the order the imported
//node tree stores them. A Skeleton uses the same order for its bones, so a bone
//index addresses the transform it drives without going through the node names,
//which are not guaranteed to be unique.
using NodeTransformTable = std::vector<Transform*>;

//maps the name of every GameObject in the hierarchy containing `node` (node
//itself included) to its transform
void CollectNodeTransforms(const Transform& node, std::unordered_map<std::string, Transform*>& out);

//resolves a node by name, starting the search at the hierarchy root of `any_node`
Transform* FindNodeTransform(const Transform& any_node, const char* name);

}
