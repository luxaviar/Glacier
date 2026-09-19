#include "Animation/SkeletonPose.h"
#include <algorithm>
#include "Math/Util.h"

namespace glacier {

void SkeletonPose::Resize(size_t bone_count) {
    positions.resize(bone_count, Vec3f::zero);
    rotations.resize(bone_count, Quaternion::identity);
    scales.resize(bone_count, Vec3f::one);
}

void SkeletonPose::Clear() {
    std::fill(positions.begin(), positions.end(), Vec3f::zero);
    std::fill(rotations.begin(), rotations.end(), Quaternion{ 0.0f, 0.0f, 0.0f, 0.0f });
    std::fill(scales.begin(), scales.end(), Vec3f::zero);
}

void SkeletonPose::ResetToRest(const Skeleton& skeleton) {
    Resize(skeleton.bone_count());

    for (size_t i = 0; i < positions.size(); ++i) {
        const auto& bone = skeleton.bone(i);
        positions[i] = bone.rest_position;
        rotations[i] = bone.rest_rotation;
        scales[i] = bone.rest_scale;
    }
}

void SkeletonPose::Accumulate(const SkeletonPose& other, float weight) {
    if (other.bone_count() != bone_count()) {
        return;
    }

    for (size_t i = 0; i < positions.size(); ++i) {
        positions[i] = positions[i] + other.positions[i] * weight;
        scales[i] = scales[i] + other.scales[i] * weight;

        Quaternion rotation = other.rotations[i];
        if (rotations[i].MagnitudeSq() > math::kEpsilon && rotation.Dot(rotations[i]) < 0.0f) {
            //keep every contribution on the same side of the hemisphere, so a
            //blend never spins the long way around
            rotation = -rotation;
        }

        rotations[i] = rotations[i] + rotation * weight;
    }
}

void SkeletonPose::Scale(float factor) {
    for (size_t i = 0; i < positions.size(); ++i) {
        positions[i] = positions[i] * factor;
        scales[i] = scales[i] * factor;
        rotations[i].Normalize();
    }
}

void SkeletonPose::AccumulateAdditive(const SkeletonPose& delta, const SkeletonPose& rest, float weight) {
    if (delta.bone_count() != bone_count() || rest.bone_count() != bone_count()) {
        return;
    }

    for (size_t i = 0; i < positions.size(); ++i) {
        positions[i] = positions[i] + (delta.positions[i] - rest.positions[i]) * weight;
        scales[i] = scales[i] + (delta.scales[i] - rest.scales[i]) * weight;

        //the rotation delta is expressed in the local space of the bone and
        //scaled towards the identity before it is composed onto the blend
        Quaternion offset = rest.rotations[i].Inverted() * delta.rotations[i];
        rotations[i] = rotations[i] * Quaternion::Slerp(Quaternion::identity, offset, weight);
    }
}

}
