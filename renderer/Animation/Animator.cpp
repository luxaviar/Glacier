#include "Animation/Animator.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include "Core/Transform.h"
#include "Core/GameObject.h"
#include "Common/Log.h"
#include "Lux/Lux.h"

namespace glacier {

LUX_IMPL(Animator, Animator)
LUX_CTOR(Animator)
LUX_FUNC_SPEC(Animator, Play, Play, bool, size_t)
LUX_FUNC_SPEC(Animator, PlayClip, PlayClip, bool, const char*)
LUX_FUNC(Animator, Stop)
LUX_FUNC(Animator, Pause)
LUX_FUNC(Animator, Resume)
LUX_FUNC(Animator, IsPlaying)
LUX_FUNC(Animator, clip_count)
LUX_FUNC(Animator, clip_name)
LUX_FUNC(Animator, duration)
LUX_FUNC(Animator, SetTime)
LUX_FUNC(Animator, SetSpeed)
LUX_FUNC(Animator, SetLoop)
LUX_PROP_FUNC_GET(Animator, time, time)
LUX_PROP_FUNC_GET(Animator, speed, speed)
LUX_PROP_FUNC_GET(Animator, loop, loop)
LUX_PROP_FUNC_GET(Animator, playing, IsPlaying)
LUX_IMPL_END

Animator::Animator(std::vector<std::shared_ptr<AnimationClip>> clips) :
    clips_(std::move(clips))
{
}

void Animator::SetClips(std::vector<std::shared_ptr<AnimationClip>> clips) {
    clips_ = std::move(clips);

    if (current_ && std::find(clips_.begin(), clips_.end(), current_) == clips_.end()) {
        Stop();
        current_ = nullptr;
    }
}

void Animator::BindNodes(Transform& root) {
    UnbindNodes();
    CollectNodes(root);

    if (nodes_.empty()) {
        LOG_WARN("Animator on '{}' bound no node", game_object() ? game_object()->name() : "<none>");
    }
}

void Animator::UnbindNodes() {
    nodes_.clear();
    bind_pose_.clear();
}

void Animator::CollectNodes(Transform& transform) {
    BindPose pose;
    pose.transform = &transform;
    pose.position = transform.local_position();
    pose.rotation = transform.local_rotation();
    pose.scale = transform.local_scale();

    bind_pose_.push_back(pose);
    nodes_.emplace(transform.game_object()->name(), &transform);

    for (auto* child : transform.children()) {
        CollectNodes(*child);
    }
}

const char* Animator::clip_name(size_t index) const {
    if (index >= clips_.size() || !clips_[index]) {
        return "";
    }

    return clips_[index]->name().c_str();
}

std::shared_ptr<AnimationClip> Animator::GetClip(size_t index) const {
    if (index >= clips_.size()) {
        return nullptr;
    }

    return clips_[index];
}

std::shared_ptr<AnimationClip> Animator::GetClip(const char* name) const {
    if (!name) return nullptr;

    for (const auto& clip : clips_) {
        if (clip && clip->name() == name) {
            return clip;
        }
    }

    return nullptr;
}

bool Animator::Play(size_t index) {
    return Play(index, loop_);
}

bool Animator::Play(size_t index, bool loop) {
    auto clip = GetClip(index);
    if (!clip || clip->track_count() == 0) {
        LOG_WARN("Animator::Play failed, invalid clip index {}", index);
        return false;
    }

    current_ = clip;
    loop_ = loop;
    time_ = 0.0f;
    playing_ = true;

    Apply(time_);
    return true;
}

bool Animator::PlayClip(const char* name) {
    return PlayClip(name, loop_);
}

bool Animator::PlayClip(const char* name, bool loop) {
    auto clip = GetClip(name);
    if (!clip) {
        LOG_WARN("Animator::PlayClip failed, unknown clip '{}'", name ? name : "<none>");
        return false;
    }

    current_ = clip;
    loop_ = loop;
    time_ = 0.0f;
    playing_ = true;

    Apply(time_);
    return true;
}

void Animator::Stop() {
    playing_ = false;
    time_ = 0.0f;

    RestoreBindPose();
}

void Animator::Pause() {
    playing_ = false;
}

void Animator::Resume() {
    if (!current_) {
        Play(0);
        return;
    }

    playing_ = true;
}

void Animator::SetTime(float t) {
    time_ = std::max(t, 0.0f);
    Apply(time_);
}

float Animator::duration() const {
    return current_ ? current_->duration() : 0.0f;
}

void Animator::LateUpdate(float dt) {
    if (!playing_ || !current_) {
        return;
    }

    float clip_duration = current_->duration();
    time_ += dt * speed_;

    if (clip_duration > 0.0f) {
        if (loop_) {
            time_ = std::fmod(time_, clip_duration);
            if (time_ < 0.0f) {
                time_ += clip_duration;
            }
        }
        else {
            time_ = std::clamp(time_, 0.0f, clip_duration);
            if (time_ >= clip_duration) {
                playing_ = false;
            }
        }
    }
    else {
        time_ = 0.0f;
    }

    Apply(time_);
}

void Animator::ApplyPose(const NodeTrack& track, const NodePose& pose) const {
    auto it = nodes_.find(track.node_name());
    if (it == nodes_.end()) {
        return;
    }

    auto& transform = *it->second;

    if (pose.has_position) transform.local_position(pose.position);
    if (pose.has_rotation) transform.local_rotation(pose.rotation);
    if (pose.has_scale) transform.local_scale(pose.scale);
}

void Animator::Apply(float time) const {
    if (!current_) return;

    for (const auto& track : current_->tracks()) {
        NodePose pose;
        if (track.Sample(time, pose)) {
            ApplyPose(track, pose);
        }
    }
}

void Animator::RestoreBindPose() const {
    for (const auto& pose : bind_pose_) {
        pose.transform->local_position(pose.position);
        pose.transform->local_rotation(pose.rotation);
        pose.transform->local_scale(pose.scale);
    }
}

void Animator::DrawInspector() {
    if (!ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    ImGui::Text("clips: %d  nodes: %d", (int)clips_.size(), (int)nodes_.size());

    if (!clips_.empty()) {
        const char* preview = current_ ? current_->name().c_str() : "<none>";
        if (ImGui::BeginCombo("##clip", preview)) {
            for (size_t i = 0; i < clips_.size(); ++i) {
                bool selected = clips_[i] == current_;
                if (ImGui::Selectable(clip_name(i), selected)) {
                    Play(i, loop_);
                }

                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }
    }

    if (ImGui::Button(playing_ ? "Pause" : "Play")) {
        if (playing_) {
            Pause();
        }
        else {
            Resume();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        Stop();
    }

    float clip_duration = duration();
    if (clip_duration > 0.0f) {
        float t = time_;
        if (ImGui::SliderFloat("time", &t, 0.0f, clip_duration)) {
            SetTime(t);
        }
    }

    ImGui::Checkbox("loop", &loop_);
    ImGui::InputFloat("speed", &speed_);
}

}
