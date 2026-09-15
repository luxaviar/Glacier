#pragma once

#include <deque>
#include <string>
#include "Animation/NodeTrack.h"

namespace glacier {

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

private:
    std::string name_;
    std::deque<NodeTrack> tracks_;
};

}
