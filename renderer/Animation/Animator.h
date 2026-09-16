#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Core/Behaviour.h"
#include "Animation/AnimationClip.h"
#include "Animation/AnimationKeyframe.h"

namespace glacier {

class Transform;

//Plays a single AnimationClip on a hierarchy of Transforms, resolved by node name.
//Sampling runs in LateUpdate so gameplay code in Update can drive it first.
class Animator : public Behaviour {
public:
    Animator() = default;
    explicit Animator(std::vector<std::shared_ptr<AnimationClip>> clips);
    ~Animator() override = default;

    void SetClips(std::vector<std::shared_ptr<AnimationClip>> clips);
    const std::vector<std::shared_ptr<AnimationClip>>& clips() const { return clips_; }

    //Collects every transform under root by name and remembers the bind pose,
    //so every clip can be retargeted onto this instance.
    void BindNodes(Transform& root);
    void UnbindNodes();
    size_t node_count() const { return nodes_.size(); }

    size_t clip_count() const { return clips_.size(); }
    const char* clip_name(size_t index) const;
    std::shared_ptr<AnimationClip> GetClip(size_t index) const;
    std::shared_ptr<AnimationClip> GetClip(const char* name) const;

    //Restarts the clip from the beginning.
    bool Play(size_t index);
    bool Play(size_t index, bool loop);
    bool PlayClip(const char* name);
    bool PlayClip(const char* name, bool loop);

    //Stops playback and restores the bind pose.
    void Stop();
    void Pause();
    void Resume();
    bool IsPlaying() const { return playing_; }

    float time() const { return time_; }
    //Also applies the pose, so the inspector can scrub while paused.
    void SetTime(float t);

    float speed() const { return speed_; }
    void SetSpeed(float v) { speed_ = v; }

    bool loop() const { return loop_; }
    void SetLoop(bool v) { loop_ = v; }

    float duration() const;

    void LateUpdate(float dt) override;
    void DrawInspector() override;

private:
    struct BindPose {
        Transform* transform = nullptr;
        Vec3f position;
        Quaternion rotation;
        Vec3f scale;
    };

    void ApplyPose(const NodeTrack& track, const NodePose& pose) const;
    void Apply(float time) const;
    void RestoreBindPose() const;

    std::vector<std::shared_ptr<AnimationClip>> clips_;
    std::unordered_map<std::string, Transform*> nodes_;
    std::vector<BindPose> bind_pose_;

    std::shared_ptr<AnimationClip> current_;
    float time_ = 0.0f;
    float speed_ = 1.0f;
    bool loop_ = true;
    bool playing_ = false;
};

}
