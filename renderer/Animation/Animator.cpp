#include "Animation/Animator.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include "Core/Transform.h"
#include "Core/GameObject.h"
#include "Animation/NodeLookup.h"
#include "Common/Log.h"
#include "Lux/Lux.h"

namespace glacier {

LUX_IMPL(Animator, Animator)
LUX_CTOR(Animator)
LUX_FUNC_SPEC(Animator, Play, Play, bool, size_t)
LUX_FUNC_SPEC(Animator, PlayClip, PlayClip, bool, const char*)
LUX_FUNC_SPEC(Animator, CrossFade, CrossFade, bool, const char*, float)
LUX_FUNC_SPEC(Animator, CrossFade, CrossFadeIndex, bool, size_t, float)
LUX_FUNC_SPEC(Animator, SetWeight, SetWeight, bool, const char*, float, bool)
LUX_FUNC_SPEC(Animator, SetWeight, SetWeightIndex, bool, size_t, float, bool)
LUX_FUNC(Animator, GetWeight)
LUX_FUNC(Animator, Stop)
LUX_FUNC(Animator, Pause)
LUX_FUNC(Animator, Resume)
LUX_FUNC(Animator, IsPlaying)
LUX_FUNC(Animator, clip_count)
LUX_FUNC(Animator, clip_name)
LUX_FUNC(Animator, bone_count)
LUX_FUNC(Animator, active_clip_count)
LUX_FUNC(Animator, active_clip_name)
LUX_FUNC(Animator, active_clip_weight)
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

    auto unknown = [this](const std::shared_ptr<AnimationClip>& clip) {
        return std::find(clips_.begin(), clips_.end(), clip) == clips_.end();
    };

    actions_.erase(std::remove_if(actions_.begin(), actions_.end(),
        [&](const Action& action) { return unknown(action.clip); }), actions_.end());

    if (current_ && unknown(current_)) {
        Stop();
        current_ = nullptr;
    }
}

void Animator::SetSkeleton(std::shared_ptr<Skeleton> skeleton) {
    skeleton_ = std::move(skeleton);
    owns_skeleton_ = false;

    if (!nodes_.empty()) {
        ResolveBones();
    }
}

void Animator::BindNodes(Transform& root) {
    UnbindNodes();
    CollectNodeTransforms(root, nodes_);

    //remember the bind pose so that Stop() can restore it
    bind_pose_.reserve(nodes_.size());
    for (const auto& [name, transform] : nodes_) {
        BindPose pose;
        pose.transform = transform;
        pose.position = transform->local_position();
        pose.rotation = transform->local_rotation();
        pose.scale = transform->local_scale();
        bind_pose_.push_back(pose);
    }

    if (nodes_.empty()) {
        LOG_WARN("Animator on '{}' bound no node", game_object() ? game_object()->name() : "<none>");
    }

    if (!skeleton_) {
        //without an imported skeleton every node under the root becomes a bone,
        //which lets pure node animation be mixed as well
        skeleton_ = Skeleton::FromHierarchy(root);
        owns_skeleton_ = true;
    }

    ResolveBones();
    ReportUnknownTracks();
}

void Animator::UnbindNodes() {
    nodes_.clear();
    bind_pose_.clear();
    bone_transforms_.clear();
    animated_position_.clear();
    animated_rotation_.clear();
    animated_scale_.clear();
    written_position_.clear();
    written_rotation_.clear();
    written_scale_.clear();

    if (owns_skeleton_) {
        skeleton_ = nullptr;
        owns_skeleton_ = false;
    }
}

void Animator::ResolveBones() {
    if (!skeleton_) {
        return;
    }

    size_t count = skeleton_->bone_count();
    bone_transforms_.assign(count, nullptr);
    animated_position_.assign(count, false);
    animated_rotation_.assign(count, false);
    animated_scale_.assign(count, false);
    written_position_.assign(count, false);
    written_rotation_.assign(count, false);
    written_scale_.assign(count, false);

    size_t missing = 0;
    for (size_t i = 0; i < count; ++i) {
        auto it = nodes_.find(skeleton_->bone(i).name);
        if (it != nodes_.end()) {
            bone_transforms_[i] = it->second;
        }
        else {
            ++missing;
        }
    }

    pose_.Resize(count);
    scratch_.Resize(count);
    rest_.ResetToRest(*skeleton_);

    if (missing > 0) {
        LOG_WARN("Animator '{}': {} of {} skeleton bones have no transform in the hierarchy",
            game_object() ? game_object()->name() : "<none>", missing, count);
    }
}

void Animator::ReportUnknownTracks() const {
    if (!skeleton_) {
        return;
    }

    for (const auto& clip : clips_) {
        if (!clip) {
            continue;
        }

        size_t unknown = 0;
        for (const auto& track : clip->tracks()) {
            if (skeleton_->IndexOf(track.node_name().c_str()) == kInvalidBoneIndex) {
                ++unknown;
            }
        }

        if (unknown > 0) {
            LOG_WARN("Animator '{}': clip '{}' animates {} node(s) that are not part of the skeleton",
                game_object() ? game_object()->name() : "<none>", clip->name(), unknown);
        }
    }
}

Animator::Action* Animator::FindAction(const AnimationClip* clip) {
    for (auto& action : actions_) {
        if (action.clip.get() == clip) {
            return &action;
        }
    }

    return nullptr;
}

const Animator::Action* Animator::FindAction(const AnimationClip* clip) const {
    for (const auto& action : actions_) {
        if (action.clip.get() == clip) {
            return &action;
        }
    }

    return nullptr;
}

Animator::Action& Animator::AddAction(const std::shared_ptr<AnimationClip>& clip, float weight) {
    if (auto* existing = FindAction(clip.get())) {
        return *existing;
    }

    Action action;
    action.clip = clip;
    action.speed = speed_;
    action.loop = loop_;
    action.weight = weight;
    action.fade_from = weight;
    action.fade_to = weight;

    actions_.push_back(std::move(action));
    return actions_.back();
}

size_t Animator::IndexOfClip(const AnimationClip* clip) const {
    for (size_t i = 0; i < clips_.size(); ++i) {
        if (clips_[i].get() == clip) {
            return i;
        }
    }

    return clips_.size();
}

bool Animator::RemoveAction(const AnimationClip* clip) {
    for (auto it = actions_.begin(); it != actions_.end(); ++it) {
        if (it->clip.get() == clip) {
            actions_.erase(it);
            return true;
        }
    }

    return false;
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

const char* Animator::active_clip_name(size_t index) const {
    if (index >= actions_.size() || !actions_[index].clip) {
        return "";
    }

    return actions_[index].clip->name().c_str();
}

float Animator::active_clip_weight(size_t index) const {
    return index < actions_.size() ? actions_[index].weight : 0.0f;
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

    //an instant switch: every other clip stops
    actions_.clear();

    Action action;
    action.clip = clip;
    action.loop = loop;
    action.speed = speed_;
    action.weight = 1.0f;
    action.fade_from = 1.0f;
    action.fade_to = 1.0f;
    actions_.push_back(std::move(action));

    current_ = clip;
    loop_ = loop;
    playing_ = true;

    Evaluate();
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

    size_t index = IndexOfClip(clip.get());
    if (index >= clips_.size()) {
        return false;
    }

    return Play(index, loop);
}

bool Animator::CrossFade(size_t index, float duration) {
    auto clip = GetClip(index);
    if (!clip || clip->track_count() == 0) {
        LOG_WARN("Animator::CrossFade failed, invalid clip index {}", index);
        return false;
    }

    if (duration <= 0.0f || actions_.empty()) {
        return Play(index, loop_);
    }

    //a clip that is already playing keeps its time, so fading back to it picks
    //up where it is instead of snapping back to the first frame
    Action& target = AddAction(clip, 0.0f);
    target.playing = true;
    target.fade_from = target.weight;
    target.fade_to = 1.0f;
    target.fade_elapsed = 0.0f;
    target.fade_duration = duration;

    for (auto& action : actions_) {
        if (&action == &target) {
            continue;
        }

        action.fade_from = action.weight;
        action.fade_to = 0.0f;
        action.fade_elapsed = 0.0f;
        action.fade_duration = duration;
    }

    current_ = clip;
    playing_ = true;

    LOG_LOG("Animator '{}' crossfade to '{}' over {:.2f}s",
        game_object() ? game_object()->name() : "<none>", clip->name(), duration);

    Evaluate();
    return true;
}

bool Animator::CrossFade(const char* name, float duration) {
    auto clip = GetClip(name);
    if (!clip) {
        LOG_WARN("Animator::CrossFade failed, unknown clip '{}'", name ? name : "<none>");
        return false;
    }

    size_t index = IndexOfClip(clip.get());
    if (index >= clips_.size()) {
        return false;
    }

    return CrossFade(index, duration);
}

bool Animator::SetWeight(size_t index, float weight, bool additive) {
    auto clip = GetClip(index);
    if (!clip || clip->track_count() == 0) {
        LOG_WARN("Animator::SetWeight failed, invalid clip index {}", index);
        return false;
    }

    weight = std::max(weight, 0.0f);
    auto* action = FindAction(clip.get());

    if (weight <= 0.0f) {
        if (!action) {
            return true;
        }

        action->weight = 0.0f;
        action->fade_from = 0.0f;
        action->fade_to = 0.0f;
        action->fade_duration = 0.0f;

        //the primary clip stays around so the inspector can show and restart it
        if (action->clip != current_) {
            RemoveAction(action->clip.get());
        }

        Evaluate();
        return true;
    }

    if (!action) {
        action = &AddAction(clip, weight);
    }

    action->loop = loop_;
    action->playing = true;
    action->additive = additive;
    action->weight = weight;
    action->fade_from = weight;
    action->fade_to = weight;
    action->fade_duration = 0.0f;

    if (!current_) {
        current_ = clip;
        playing_ = true;
    }

    Evaluate();
    return true;
}

bool Animator::SetWeight(const char* name, float weight, bool additive) {
    auto clip = GetClip(name);
    if (!clip) {
        LOG_WARN("Animator::SetWeight failed, unknown clip '{}'", name ? name : "<none>");
        return false;
    }

    size_t index = IndexOfClip(clip.get());
    if (index >= clips_.size()) {
        return false;
    }

    return SetWeight(index, weight, additive);
}

float Animator::GetWeight(const char* name) const {
    auto clip = GetClip(name);
    if (!clip) {
        return 0.0f;
    }

    const auto* action = FindAction(clip.get());
    return action ? action->weight : 0.0f;
}

void Animator::Stop() {
    playing_ = false;
    actions_.clear();

    RestoreBindPose();
}

void Animator::Pause() {
    playing_ = false;
}

void Animator::Resume() {
    if (actions_.empty()) {
        size_t index = IndexOfClip(current_.get());
        Play(index < clips_.size() ? index : 0, loop_);
        return;
    }

    //a one shot that ran to its end starts over
    for (auto& action : actions_) {
        if (!action.playing && !action.loop) {
            action.time = 0.0f;
            action.playing = true;
        }
    }

    playing_ = true;
}

float Animator::time() const {
    const auto* action = FindAction(current_.get());
    return action ? action->time : 0.0f;
}

void Animator::SetTime(float t) {
    auto* action = FindAction(current_.get());
    if (!action) {
        return;
    }

    action->time = std::max(t, 0.0f);
    Evaluate();
}

void Animator::SetSpeed(float v) {
    speed_ = v;

    for (auto& action : actions_) {
        action.speed = v;
    }
}

void Animator::SetLoop(bool v) {
    loop_ = v;

    if (auto* action = FindAction(current_.get())) {
        action->loop = v;
        action->playing = true;
    }
}

float Animator::duration() const {
    return current_ ? current_->duration() : 0.0f;
}

void Animator::LateUpdate(float dt) {
    if (!playing_ || actions_.empty()) {
        return;
    }

    for (auto& action : actions_) {
        AdvanceTime(action, dt);

        if (action.fade_duration > 0.0f) {
            action.fade_elapsed = std::min(action.fade_elapsed + dt, action.fade_duration);

            float ratio = action.fade_elapsed / action.fade_duration;
            action.weight = action.fade_from + (action.fade_to - action.fade_from) * ratio;

            if (action.fade_elapsed >= action.fade_duration) {
                action.weight = action.fade_to;
                action.fade_duration = 0.0f;
            }
        }
    }

    //a clip that faded out takes no part in the blend anymore
    actions_.erase(std::remove_if(actions_.begin(), actions_.end(),
        [this](const Action& action) {
            return action.weight <= 0.0f && action.fade_duration <= 0.0f && action.clip != current_;
        }), actions_.end());

    if (actions_.empty()) {
        playing_ = false;
        return;
    }

    Evaluate();

    //stop sampling once every clip is done and no fade is left to run
    bool fading = false;
    bool running = false;
    for (const auto& action : actions_) {
        fading = fading || action.fade_duration > 0.0f;
        running = running || (action.playing && action.weight > 0.0f);
    }

    if (!fading && !running) {
        playing_ = false;
    }
}

void Animator::AdvanceTime(Action& action, float dt) {
    if (!action.playing || !action.clip) {
        return;
    }

    float clip_duration = action.clip->duration();
    if (clip_duration <= 0.0f) {
        action.time = 0.0f;
        return;
    }

    action.time += dt * action.speed;

    if (action.loop) {
        action.time = std::fmod(action.time, clip_duration);
        if (action.time < 0.0f) {
            action.time += clip_duration;
        }
    }
    else {
        action.time = std::clamp(action.time, 0.0f, clip_duration);
        if (action.time >= clip_duration) {
            action.playing = false;
        }
    }
}

void Animator::Sample(const AnimationClip& clip, float time, SkeletonPose& pose) const {
    //a clip that does not animate a channel contributes the rest pose of that
    //channel, so mixing towards it relaxes the bone instead of freezing it
    pose.ResetToRest(*skeleton_);

    for (const auto& track : clip.tracks()) {
        int32_t index = skeleton_->IndexOf(track.node_name().c_str());
        if (index == kInvalidBoneIndex) {
            continue;
        }

        NodePose node_pose;
        if (!track.Sample(time, node_pose)) {
            continue;
        }

        size_t bone = (size_t)index;
        if (node_pose.has_position) pose.positions[bone] = node_pose.position;
        if (node_pose.has_rotation) pose.rotations[bone] = node_pose.rotation;
        if (node_pose.has_scale) pose.scales[bone] = node_pose.scale;
    }
}

void Animator::MarkAnimated(const AnimationClip& clip) {
    for (const auto& track : clip.tracks()) {
        int32_t index = skeleton_->IndexOf(track.node_name().c_str());
        if (index == kInvalidBoneIndex) {
            continue;
        }

        size_t bone = (size_t)index;
        animated_position_[bone] = animated_position_[bone] || !track.positions().empty();
        animated_rotation_[bone] = animated_rotation_[bone] || !track.rotations().empty();
        animated_scale_[bone] = animated_scale_[bone] || !track.scales().empty();
    }
}

void Animator::Evaluate() {
    if (!skeleton_ || skeleton_->bone_count() == 0) {
        return;
    }

    pose_.Clear();
    std::fill(animated_position_.begin(), animated_position_.end(), false);
    std::fill(animated_rotation_.begin(), animated_rotation_.end(), false);
    std::fill(animated_scale_.begin(), animated_scale_.end(), false);

    float weight_sum = 0.0f;
    for (const auto& action : actions_) {
        if (action.weight <= 0.0f || !action.clip || action.additive) {
            continue;
        }

        Sample(*action.clip, action.time, scratch_);
        pose_.Accumulate(scratch_, action.weight);
        MarkAnimated(*action.clip);
        weight_sum += action.weight;
    }

    if (weight_sum > 0.0f) {
        pose_.Scale(1.0f / weight_sum);
    }
    else {
        pose_.ResetToRest(*skeleton_);
    }

    //additive layers are mixed on top of the finished pose
    for (const auto& action : actions_) {
        if (action.weight <= 0.0f || !action.clip || !action.additive) {
            continue;
        }

        Sample(*action.clip, action.time, scratch_);
        pose_.AccumulateAdditive(scratch_, rest_, action.weight);
        MarkAnimated(*action.clip);
    }

    Apply(pose_);
}

void Animator::Apply(const SkeletonPose& pose) {
    size_t count = std::min(bone_transforms_.size(), pose.bone_count());

    for (size_t i = 0; i < count; ++i) {
        auto* transform = bone_transforms_[i];
        if (!transform) {
            continue;
        }

        //a channel is written while an active clip drives it, and once more
        //after it stopped so that it can fall back to the rest pose
        bool position = animated_position_[i] || written_position_[i];
        bool rotation = animated_rotation_[i] || written_rotation_[i];
        bool scale = animated_scale_[i] || written_scale_[i];

        if (position && transform->local_position() != pose.positions[i]) {
            transform->local_position(pose.positions[i]);
        }

        if (rotation && transform->local_rotation() != pose.rotations[i]) {
            transform->local_rotation(pose.rotations[i]);
        }

        if (scale && transform->local_scale() != pose.scales[i]) {
            transform->local_scale(pose.scales[i]);
        }

        written_position_[i] = animated_position_[i];
        written_rotation_[i] = animated_rotation_[i];
        written_scale_[i] = animated_scale_[i];
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

    ImGui::Text("clips: %d  nodes: %d  bones: %d",
        (int)clips_.size(), (int)nodes_.size(), (int)bone_count());

    if (!clips_.empty()) {
        if (selected_clip_ >= clips_.size()) {
            selected_clip_ = 0;
        }

        const char* preview = clip_name(selected_clip_);
        if (ImGui::BeginCombo("##clip", preview)) {
            for (size_t i = 0; i < clips_.size(); ++i) {
                bool selected = i == selected_clip_;
                if (ImGui::Selectable(clip_name(i), selected)) {
                    selected_clip_ = i;
                }

                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        if (ImGui::Button("Play")) {
            Play(selected_clip_, loop_);
        }

        ImGui::SameLine();
        if (ImGui::Button("CrossFade")) {
            CrossFade(selected_clip_, fade_time_);
        }

        ImGui::SameLine();
        ImGui::InputFloat("fade", &fade_time_);
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
        float t = time();
        if (ImGui::SliderFloat("time", &t, 0.0f, clip_duration)) {
            SetTime(t);
        }
    }

    ImGui::Checkbox("loop", &loop_);
    ImGui::InputFloat("speed", &speed_);

    if (actions_.size() > 1) {
        ImGui::Text("blend");

        for (size_t i = 0; i < actions_.size(); ++i) {
            auto& action = actions_[i];
            std::string label = "##weight" + std::to_string(i);
            float weight = action.weight;

            ImGui::SliderFloat(label.c_str(), &weight, 0.0f, 1.0f);
            ImGui::SameLine();
            ImGui::Text("%s", action.clip ? action.clip->name().c_str() : "<none>");

            if (weight != action.weight) {
                action.weight = weight;
                action.fade_from = weight;
                action.fade_to = weight;
                action.fade_duration = 0.0f;
                Evaluate();
            }

            ImGui::SameLine();
            std::string additive_label = "additive##" + std::to_string(i);
            bool additive = action.additive;
            if (ImGui::Checkbox(additive_label.c_str(), &additive)) {
                action.additive = additive;
                Evaluate();
            }
        }
    }
}

}
