#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "Core/Behaviour.h"
#include "Animation/AnimationClip.h"
#include "Animation/AnimationKeyframe.h"
#include "Animation/Skeleton.h"
#include "Animation/SkeletonPose.h"

namespace glacier {

class Transform;

//Samples AnimationClips into SkeletonPoses, mixes the clips that are playing
//and applies the result to the bound Transforms. Because the clips are mixed as
//poses, the animator can crossfade or layer any number of clips of one
//skeleton; sampling runs in LateUpdate so gameplay code can drive it first.
class Animator : public Behaviour {
public:
    Animator() = default;
    explicit Animator(std::vector<std::shared_ptr<AnimationClip>> clips);
    ~Animator() override = default;

    void SetClips(std::vector<std::shared_ptr<AnimationClip>> clips);
    const std::vector<std::shared_ptr<AnimationClip>>& clips() const { return clips_; }

    //Uses an imported skeleton (bones, inverse bind matrices and rest pose)
    //instead of deriving one from the hierarchy; call it before BindNodes
    void SetSkeleton(std::shared_ptr<Skeleton> skeleton);
    const std::shared_ptr<Skeleton>& skeleton() const { return skeleton_; }
    size_t bone_count() const { return skeleton_ ? skeleton_->bone_count() : 0; }

    //Collects every transform under root by name, remembers the bind pose and
    //derives a skeleton when none was imported, so every clip can be retargeted
    //onto this instance.
    void BindNodes(Transform& root);
    void UnbindNodes();
    size_t node_count() const { return nodes_.size(); }

    size_t clip_count() const { return clips_.size(); }
    const char* clip_name(size_t index) const;
    std::shared_ptr<AnimationClip> GetClip(size_t index) const;
    std::shared_ptr<AnimationClip> GetClip(const char* name) const;

    //Restarts the clip from the beginning; the other clips stop.
    bool Play(size_t index);
    bool Play(size_t index, bool loop);
    bool PlayClip(const char* name);
    bool PlayClip(const char* name, bool loop);

    //Blends from whatever is playing into the given clip over `duration`
    //seconds, the outgoing clips fade out over the same span. The pose is
    //unchanged on the frame the fade starts, so there is no jump.
    bool CrossFade(size_t index, float duration);
    bool CrossFade(const char* name, float duration);

    //Layers a clip into the blend or removes it when the weight is 0. What gets
    //applied is the normalized weighted average of the active clips, which is
    //stable for any number of clips of the same skeleton.
    bool SetWeight(size_t index, float weight, bool additive = false);
    bool SetWeight(const char* name, float weight, bool additive = false);
    float GetWeight(const char* name) const;
    size_t active_clip_count() const { return actions_.size(); }
    const char* active_clip_name(size_t index) const;
    float active_clip_weight(size_t index) const;

    //Stops playback and restores the bind pose.
    void Stop();
    void Pause();
    void Resume();
    bool IsPlaying() const { return playing_; }

    //the primary clip is the one played or crossfaded to last
    float time() const;
    //Also applies the pose, so the inspector can scrub while paused.
    void SetTime(float t);

    float speed() const { return speed_; }
    void SetSpeed(float v);

    bool loop() const { return loop_; }
    void SetLoop(bool v);

    float duration() const;
    //the blended pose of the last evaluation
    const SkeletonPose& pose() const { return pose_; }

    void LateUpdate(float dt) override;
    void DrawInspector() override;

private:
    //one playing clip and its place in the blend
    struct Action {
        std::shared_ptr<AnimationClip> clip;
        float time = 0.0f;
        float speed = 1.0f;
        bool loop = true;
        bool playing = true;
        //additive clips are layered on top of the mixed pose instead of being
        //averaged into it
        bool additive = false;
        float weight = 1.0f;
        float fade_from = 1.0f;
        float fade_to = 1.0f;
        float fade_elapsed = 0.0f;
        float fade_duration = 0.0f;
    };

    struct BindPose {
        Transform* transform = nullptr;
        Vec3f position;
        Quaternion rotation;
        Vec3f scale;
    };

    Action* FindAction(const AnimationClip* clip);
    const Action* FindAction(const AnimationClip* clip) const;
    Action& AddAction(const std::shared_ptr<AnimationClip>& clip, float weight);
    size_t IndexOfClip(const AnimationClip* clip) const;
    bool RemoveAction(const AnimationClip* clip);

    void ResolveBones();
    void ReportUnknownTracks() const;
    void Sample(const AnimationClip& clip, float time, SkeletonPose& pose) const;
    void MarkAnimated(const AnimationClip& clip);
    void Evaluate();
    void Apply(const SkeletonPose& pose);
    void AdvanceTime(Action& action, float dt);
    void RestoreBindPose() const;

    std::vector<std::shared_ptr<AnimationClip>> clips_;
    std::shared_ptr<AnimationClip> current_;

    std::shared_ptr<Skeleton> skeleton_;
    //a skeleton derived from the hierarchy goes away with the binding
    bool owns_skeleton_ = false;
    std::vector<Transform*> bone_transforms_;

    std::unordered_map<std::string, Transform*> nodes_;
    std::vector<BindPose> bind_pose_;

    //channels an active clip drives, and channels written during the last
    //evaluation (they are written once more so they can fall back to rest)
    std::vector<bool> animated_position_;
    std::vector<bool> animated_rotation_;
    std::vector<bool> animated_scale_;
    std::vector<bool> written_position_;
    std::vector<bool> written_rotation_;
    std::vector<bool> written_scale_;

    std::vector<Action> actions_;
    SkeletonPose pose_;
    SkeletonPose scratch_;
    SkeletonPose rest_;

    float speed_ = 1.0f;
    bool loop_ = true;
    bool playing_ = false;

    size_t selected_clip_ = 0;
    float fade_time_ = 0.5f;
};

}
