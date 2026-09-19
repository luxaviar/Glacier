#pragma once

#include <vector>
#include "Animation/Skeleton.h"
#include "Math/Quat.h"
#include "Math/Vec3.h"

namespace glacier {

//The TRS of every bone of a Skeleton. A pose is a plain value, so clips are
//sampled into scratch poses and mixed there; nothing touches the Transform
//tree until the blend is finished.
struct SkeletonPose {
    std::vector<Vec3f> positions;
    std::vector<Quaternion> rotations;
    std::vector<Vec3f> scales;

    void Resize(size_t bone_count);
    size_t bone_count() const { return positions.size(); }

    //zeroes every channel; accumulating into a cleared pose is only meaningful
    //after Scale()
    void Clear();

    //The rest pose of the skeleton; this is what a channel falls back to while
    //the mixed clips do not animate it.
    void ResetToRest(const Skeleton& skeleton);

    //this += other * weight; rotations take the shortest arc towards the
    //accumulated rotation
    void Accumulate(const SkeletonPose& other, float weight);

    //this += (delta - rest) * weight. A channel the delta clip does not animate
    //equals its rest value and therefore adds nothing.
    void AccumulateAdditive(const SkeletonPose& delta, const SkeletonPose& rest, float weight);

    //divides the accumulated weights out and normalizes the rotations
    void Scale(float factor);
};

}
