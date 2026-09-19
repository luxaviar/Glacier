#include "Animation/Skeleton.h"
#include "Core/GameObject.h"
#include "Core/Transform.h"

namespace glacier {

namespace {

//fnv-1a, so a changed name or parent changes the signature of the skeleton
constexpr uint64_t kFnvOffset = 14695981039346656037ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

void HashByte(uint64_t& hash, uint8_t byte) {
    hash = (hash ^ byte) * kFnvPrime;
}

void HashValue(uint64_t& hash, int32_t value) {
    for (int i = 0; i < 4; ++i) {
        HashByte(hash, (uint8_t)((uint32_t)value >> (i * 8)));
    }
}

void HashString(uint64_t& hash, const std::string& value) {
    for (char c : value) {
        HashByte(hash, (uint8_t)c);
    }

    HashByte(hash, 0);
}

void CollectHierarchy(const Transform& node, int32_t parent, Skeleton& skeleton) {
    int32_t index = skeleton.AddBone(node.game_object()->name().c_str(), parent,
        node.local_position(), node.local_rotation(), node.local_scale());

    for (auto* child : node.children()) {
        CollectHierarchy(*child, index, skeleton);
    }
}

}

std::shared_ptr<Skeleton> Skeleton::FromHierarchy(const Transform& root) {
    auto skeleton = std::make_shared<Skeleton>();
    CollectHierarchy(root, kInvalidBoneIndex, *skeleton);
    skeleton->Build();

    return skeleton;
}

int32_t Skeleton::AddBone(const char* name, int32_t parent,
    const Vec3f& position, const Quaternion& rotation, const Vec3f& scale,
    const Matrix4x4& inverse_bind) {
    SkeletonBone bone;
    bone.name = name ? name : "";
    bone.parent = parent;
    bone.inverse_bind = inverse_bind;
    bone.rest_position = position;
    bone.rest_rotation = rotation;
    bone.rest_scale = scale;

    bones_.push_back(std::move(bone));
    return (int32_t)bones_.size() - 1;
}

void Skeleton::Build() {
    lookup_.clear();
    lookup_.reserve(bones_.size());

    signature_ = kFnvOffset;

    for (size_t i = 0; i < bones_.size(); ++i) {
        //clips address nodes by name, so a duplicated name always resolves to
        //the first bone that carries it; bones themselves bind by index
        lookup_.emplace(bones_[i].name, (int32_t)i);

        HashString(signature_, bones_[i].name);
        HashValue(signature_, bones_[i].parent);
    }
}

int32_t Skeleton::IndexOf(const char* name) const {
    if (!name) {
        return kInvalidBoneIndex;
    }

    auto it = lookup_.find(name);
    return it != lookup_.end() ? it->second : kInvalidBoneIndex;
}

}
