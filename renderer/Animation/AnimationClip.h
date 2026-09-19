#pragma once

#include <deque>
#include <string>
#include "Animation/NodeTrack.h"

namespace glacier {

class Skeleton;

//a named set of node tracks; clips are immutable once imported and can be shared
class AnimationClip {
public:
    explicit AnimationClip(const char* name = "Animation");

    const std::string& name() const { return name_; }
    void name(const char* v) { name_ = v; }

    //seconds; the largest track duration
    float duration() const;

    size_t track_count() const { return tracks_.size(); }
    const std::deque<NodeTrack>& tracks() const { return tracks_; }

    //returns the existing track when the node is already animated, so that
    //channels of a node coming from different sources get merged.
    //tracks are stored in a deque, so the returned reference stays valid while
    //more tracks are added
    NodeTrack& AddTrack(const char* node_name);
    const NodeTrack* FindTrack(const char* node_name) const;

    //Resolves the node of every track to a bone of `skeleton`, so sampling does
    //not have to look the names up again. The signature it stores says which
    //skeleton the tracks were bound to; another skeleton (a retarget) has to fall
    //back to the names.
    void BindToSkeleton(const Skeleton& skeleton);
    uint64_t skeleton_signature() const { return skeleton_signature_; }

private:
    std::string name_;
    std::deque<NodeTrack> tracks_;
    uint64_t skeleton_signature_ = 0;
};

}
