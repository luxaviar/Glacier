#pragma once

#include <cstdint>
#include "Math/Vec3.h"
#include "Math/Quat.h"

namespace glacier {

//interpolation of the segment starting at this keyframe (matches glTF semantics)
enum class AnimationInterpolation : uint8_t {
    kStep,
    kLinear,
};

struct Vec3Keyframe {
    float time = 0.0f; //seconds
    Vec3f value;
    AnimationInterpolation interpolation = AnimationInterpolation::kLinear;
};

struct QuatKeyframe {
    float time = 0.0f; //seconds
    Quaternion value;
    AnimationInterpolation interpolation = AnimationInterpolation::kLinear;
};

//local transform sampled from a track; a channel is valid only when its flag is set
struct NodePose {
    bool has_position = false;
    bool has_rotation = false;
    bool has_scale = false;

    Vec3f position;
    Quaternion rotation;
    Vec3f scale;
};

}
