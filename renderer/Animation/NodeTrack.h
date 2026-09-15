#pragma once

#include <string>
#include <vector>
#include "Animation/AnimationKeyframe.h"

namespace glacier {

//keyframe curves of one animated node, addressed by node name
class NodeTrack {
public:
    explicit NodeTrack(const char* node_name = "");

    const std::string& node_name() const { return node_name_; }
    void node_name(const std::string& v) { node_name_ = v; }

    void AddPosition(const Vec3Keyframe& key) { positions_.push_back(key); }
    void AddRotation(const QuatKeyframe& key) { rotations_.push_back(key); }
    void AddScale(const Vec3Keyframe& key) { scales_.push_back(key); }

    const std::vector<Vec3Keyframe>& positions() const { return positions_; }
    const std::vector<QuatKeyframe>& rotations() const { return rotations_; }
    const std::vector<Vec3Keyframe>& scales() const { return scales_; }

    bool Empty() const;
    //seconds; the largest key time of this track
    float duration() const;

    //writes every channel the track has into pose, returns false when the track is empty
    bool Sample(float time, NodePose& pose) const;

private:
    std::string node_name_;
    std::vector<Vec3Keyframe> positions_;
    std::vector<QuatKeyframe> rotations_;
    std::vector<Vec3Keyframe> scales_;
};

}
