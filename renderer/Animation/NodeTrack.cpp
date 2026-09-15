#include "Animation/NodeTrack.h"
#include <algorithm>

namespace glacier {

namespace {

bool SampleVec3(const std::vector<Vec3Keyframe>& keys, float time, Vec3f& value) {
    if (keys.empty()) return false;

    if (time <= keys.front().time) {
        value = keys.front().value;
        return true;
    }

    if (time >= keys.back().time) {
        value = keys.back().value;
        return true;
    }

    auto next = std::upper_bound(keys.begin(), keys.end(), time,
        [](float t, const Vec3Keyframe& key) { return t < key.time; });

    const auto& a = *(next - 1);
    const auto& b = *next;

    if (a.interpolation == AnimationInterpolation::kStep) {
        value = a.value;
        return true;
    }

    float span = b.time - a.time;
    float ratio = span > 0.0f ? (time - a.time) / span : 0.0f;
    value = Vec3f::Lerp(a.value, b.value, ratio);
    return true;
}

bool SampleQuat(const std::vector<QuatKeyframe>& keys, float time, Quaternion& value) {
    if (keys.empty()) return false;

    if (time <= keys.front().time) {
        value = keys.front().value;
        return true;
    }

    if (time >= keys.back().time) {
        value = keys.back().value;
        return true;
    }

    auto next = std::upper_bound(keys.begin(), keys.end(), time,
        [](float t, const QuatKeyframe& key) { return t < key.time; });

    const auto& a = *(next - 1);
    const auto& b = *next;

    if (a.interpolation == AnimationInterpolation::kStep) {
        value = a.value;
        return true;
    }

    float span = b.time - a.time;
    float ratio = span > 0.0f ? (time - a.time) / span : 0.0f;
    value = Quaternion::Slerp(a.value, b.value, ratio);
    return true;
}

}

NodeTrack::NodeTrack(const char* node_name) :
    node_name_(node_name ? node_name : "")
{
}

bool NodeTrack::Empty() const {
    return positions_.empty() && rotations_.empty() && scales_.empty();
}

float NodeTrack::duration() const {
    float duration = 0.0f;

    if (!positions_.empty()) duration = std::max(duration, positions_.back().time);
    if (!rotations_.empty()) duration = std::max(duration, rotations_.back().time);
    if (!scales_.empty()) duration = std::max(duration, scales_.back().time);

    return duration;
}

bool NodeTrack::Sample(float time, NodePose& pose) const {
    bool sampled = false;

    if (SampleVec3(positions_, time, pose.position)) {
        pose.has_position = true;
        sampled = true;
    }

    if (SampleQuat(rotations_, time, pose.rotation)) {
        pose.has_rotation = true;
        sampled = true;
    }

    if (SampleVec3(scales_, time, pose.scale)) {
        pose.has_scale = true;
        sampled = true;
    }

    return sampled;
}

}
