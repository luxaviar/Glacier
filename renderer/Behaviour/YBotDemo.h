#pragma once

#include <cstddef>
#include <cstdint>
#include "Core/Behaviour.h"

namespace glacier {

class Animator;

//Drives a Y Bot of the demo scene so the animation features of the engine can be
//seen on a real humanoid rig instead of on the test geometry:
//
//  kTimeline   - the three phases below play one after another and repeat
//  kBlendTree  - idle, walking and running sit in a 1D blend tree and the
//                parameter ramps up and down, so the pose is always a mix of
//                the two neighbouring clips
//  kCrossFade  - the same clips take turns, every switch fading the outgoing
//                pose out over the incoming one
//  kRootMotion - a turn plays as a one shot and the motion its root bone sampled
//                is applied to the entity, which is what a controller does
class YBotDemo : public Behaviour {
public:
    enum class Mode : uint8_t {
        kTimeline,
        kBlendTree,
        kCrossFade,
        kRootMotion
    };

    void SetMode(Mode mode) { mode_ = mode; }
    Mode mode() const { return mode_; }

    //one entry point per mode, an enum does not cross the Lua binding
    void UseTimeline() { SetMode(Mode::kTimeline); }
    void UseBlendTree() { SetMode(Mode::kBlendTree); }
    void UseCrossFade() { SetMode(Mode::kCrossFade); }
    void UseRootMotion() { SetMode(Mode::kRootMotion); }

    //parameter of the mix goes from low to high and back within `period` seconds,
    //which is what the blend phase of the timeline uses as well
    void SetBlendRange(float low, float high, float period) {
        blend_low_ = low;
        blend_high_ = high;
        blend_period_ = period > 0.0f ? period : 1.0f;
    }

    void Update(float dt) override;

private:
    //the three features as phases of the timeline
    enum class Phase : uint8_t {
        kNone,
        kBlendTree,
        kCrossFade,
        kTurn
    };

    void Begin(Animator& animator);
    void EnterPhase(Animator& animator, Phase phase);
    void UpdateBlendTree(Animator& animator);
    void UpdateCrossFade(Animator& animator);
    void UpdateTurn(Animator& animator);

    Animator* animator_ = nullptr;
    Mode mode_ = Mode::kTimeline;
    Phase phase_ = Phase::kNone;
    bool begun_ = false;
    float time_ = 0.0f;
    //time inside the phase that is playing
    float phase_time_ = 0.0f;
    float blend_low_ = 0.0f;
    float blend_high_ = 2.0f;
    float blend_period_ = 8.0f;
    size_t crossfade_step_ = 0;
    size_t turn_step_ = 0;
    //the turn phase faded back into idle before it ran out
    bool turn_wrapped_ = false;
    //what the turns of the current phase moved and turned the entity by
    float turn_distance_ = 0.0f;
    float turn_angle_ = 0.0f;
};

}
