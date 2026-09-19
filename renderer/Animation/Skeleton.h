#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Math/Mat4.h"
#include "Math/Quat.h"
#include "Math/Vec3.h"

namespace glacier {

class Transform;

//returned for a name that is not part of a skeleton
constexpr int32_t kInvalidBoneIndex = -1;

//one joint of a flat skeleton: the rest pose of the joint in the space of its
//parent, and the inverse bind matrix that maps a mesh space vertex into the
//local space of the joint
struct SkeletonBone {
    std::string name;
    int32_t parent = kInvalidBoneIndex;
    Matrix4x4 inverse_bind = Matrix4x4::identity;
    Vec3f rest_position;
    Quaternion rest_rotation;
    Vec3f rest_scale = Vec3f::one;
};

//A flat view of a node hierarchy: a bone only knows the index of its parent,
//so poses can be sampled, blended and evaluated without walking the Transform
//tree. Bones are stored parents first, which makes the index order a valid
//evaluation order for forward kinematics.
class Skeleton {
public:
    //Builds a skeleton from a bound hierarchy; the rest pose is the current
    //local transform of every node and there is no skinning data, so the
    //result can be used for pure node animation rigs.
    static std::shared_ptr<Skeleton> FromHierarchy(const Transform& root);

    //Bones have to be added parents first; returns the index of the new bone.
    int32_t AddBone(const char* name, int32_t parent,
        const Vec3f& position, const Quaternion& rotation, const Vec3f& scale,
        const Matrix4x4& inverse_bind = Matrix4x4::identity);

    //Builds the name lookup; call once after the last AddBone.
    void Build();

    size_t bone_count() const { return bones_.size(); }
    const std::vector<SkeletonBone>& bones() const { return bones_; }
    const SkeletonBone& bone(size_t index) const { return bones_[index]; }

    //kInvalidBoneIndex when the name is not part of the skeleton
    int32_t IndexOf(const char* name) const;

private:
    std::vector<SkeletonBone> bones_;
    std::unordered_map<std::string, int32_t> lookup_;
};

}
