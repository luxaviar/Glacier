#include "Animation/AnimationClip.h"
#include "Animation/Skeleton.h"
#include <algorithm>

namespace glacier {

AnimationClip::AnimationClip(const char* name) :
    name_(name ? name : "Animation")
{
}

float AnimationClip::duration() const {
    float duration = 0.0f;

    for (const auto& track : tracks_) {
        duration = std::max(duration, track.duration());
    }

    return duration;
}

NodeTrack& AnimationClip::AddTrack(const char* node_name) {
    const char* name = node_name ? node_name : "";

    for (auto& track : tracks_) {
        if (track.node_name() == name) {
            return track;
        }
    }

    tracks_.emplace_back(name);
    return tracks_.back();
}

void AnimationClip::AddEvent(float time, const char* name) {
    Event event;
    event.time = std::max(time, 0.0f);
    event.name = name ? name : "";

    for (const auto& existing : events_) {
        if (existing.time == event.time && existing.name == event.name) {
            return;
        }
    }

    auto it = std::upper_bound(events_.begin(), events_.end(), event.time,
        [](float time, const Event& other) { return time < other.time; });
    events_.insert(it, std::move(event));
}

const NodeTrack* AnimationClip::FindTrack(const char* node_name) const {
    if (!node_name) return nullptr;

    for (const auto& track : tracks_) {
        if (track.node_name() == node_name) {
            return &track;
        }
    }

    return nullptr;
}

void AnimationClip::BindToSkeleton(const Skeleton& skeleton) {
    skeleton_signature_ = skeleton.signature();

    for (auto& track : tracks_) {
        track.bone(skeleton.IndexOf(track.node_name().c_str()));
    }
}

}
