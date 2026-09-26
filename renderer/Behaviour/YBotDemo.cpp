#include "Behaviour/YBotDemo.h"

#include <cmath>

#include "Animation/Animator.h"
#include "Common/Log.h"
#include "Core/GameObject.h"
#include "Core/Transform.h"
#include "Lux/Lux.h"
#include "Math/Util.h"

namespace glacier {

LUX_IMPL(YBotDemo, YBotDemo)
LUX_CTOR(YBotDemo)
LUX_FUNC(YBotDemo, UseTimeline)
LUX_FUNC(YBotDemo, UseBlendTree)
LUX_FUNC(YBotDemo, UseCrossFade)
LUX_FUNC(YBotDemo, UseRootMotion)
LUX_FUNC_SPEC(YBotDemo, SetBlendRange, SetBlendRange, void, float, float, float)
LUX_IMPL_END

namespace {

//the clips are named after the Mixamo files the glTF was built from, see
//tools/mixamo_to_gltf.py
constexpr const char* kIdle = "idle";
constexpr const char* kWalking = "walking";
constexpr const char* kRunning = "running";

constexpr float kBlendThresholds[] = { 0.0f, 1.0f, 2.0f };
constexpr float kTau = 6.283185307179586f;

constexpr const char* kCrossFadeClips[] = { kIdle, kWalking, kRunning };
//the clips take turns over the phase and it ends back on idle, so the phase
//after it starts from the pose this one left behind
constexpr const char* kCrossFadeCycle[] = { kIdle, kWalking, kRunning, kIdle };
//how long one clip holds before the next fade starts, and how long the fade is
constexpr float kCrossFadeHold = 1.25f;
constexpr float kCrossFadeDuration = 1.0f;

//both clips turn the character to the left, one by ninety degrees and one by a
//half turn, so the entity of the demo keeps pivoting instead of turning back
constexpr const char* kRootMotionClips[] = { "left turn 90", "left turn" };
//the clips keep the motion of the character on the hips, and the animator hands
//it to the game once the bone is chosen
constexpr const char* kRootMotionBone = "mixamorig:Hips";
//the rig lives under a root node that scales it from centimetres to metres
constexpr float kRigScale = 0.01f;

//length of the three phases the timeline plays one after another
constexpr float kBlendPhaseTime = 7.0f;
constexpr float kCrossFadePhaseTime = 5.0f;
constexpr float kTurnPhaseTime = 3.8f;
constexpr float kTimelinePeriod = kBlendPhaseTime + kCrossFadePhaseTime + kTurnPhaseTime;
//how long a phase takes to fade into the clip it starts with
constexpr float kPhaseFade = 0.4f;
//the turn phase fades back into idle before it hands over to the blend tree
constexpr float kTurnWrap = 0.6f;

}

void YBotDemo::Update(float dt) {
    //the first frame after the scene loaded carries the whole load time, and a
    //hitch should not skip the timeline ahead
    dt = std::min(dt, 0.1f);

    if (!animator_) {
        animator_ = game_object() ? game_object()->GetComponent<Animator>() : nullptr;
        if (!animator_) {
            LOG_WARN("YBotDemo: '{}' has no animator", game_object() ? game_object()->name() : "<none>");
            return;
        }
    }

    if (!begun_) {
        Begin(*animator_);
    }

    time_ += dt;
    phase_time_ += dt;

    if (mode_ == Mode::kTimeline) {
        //the phases of the timeline hand over from one feature to the next
        float t = std::fmod(time_, kTimelinePeriod);
        Phase phase = t < kBlendPhaseTime ? Phase::kBlendTree :
            (t < kBlendPhaseTime + kCrossFadePhaseTime ? Phase::kCrossFade : Phase::kTurn);

        if (phase != phase_) {
            EnterPhase(*animator_, phase);
        }
    }

    switch (phase_) {
    case Phase::kBlendTree:
        UpdateBlendTree(*animator_);
        break;
    case Phase::kCrossFade:
        UpdateCrossFade(*animator_);
        break;
    case Phase::kTurn:
        UpdateTurn(*animator_);
        break;
    case Phase::kNone:
        break;
    }
}

void YBotDemo::Begin(Animator& animator) {
    begun_ = true;

    LOG_LOG("YBotDemo '{}': {} clips, {} bones, mode {}",
        game_object()->name(), animator.clip_count(), animator.bone_count(), (int)mode_);

    for (size_t i = 0; i < animator.clip_count(); ++i) {
        LOG_LOG("YBotDemo '{}': clip {} = '{}'", game_object()->name(), i, animator.clip_name(i));
    }

    switch (mode_) {
    case Mode::kBlendTree:
        EnterPhase(animator, Phase::kBlendTree);
        break;
    case Mode::kCrossFade:
        EnterPhase(animator, Phase::kCrossFade);
        break;
    case Mode::kRootMotion:
        EnterPhase(animator, Phase::kTurn);
        break;
    case Mode::kTimeline:
        break;
    }
}

void YBotDemo::EnterPhase(Animator& animator, Phase phase) {
    phase_ = phase;
    phase_time_ = 0.0f;

    switch (phase) {
    case Phase::kBlendTree: {
        //every clip of the file drives the same skeleton, so the animator can
        //mix them; the parameter picks the two neighbours that take part
        animator.ClearBlendClips();
        animator.SetRootMotion(false);
        animator.SetLoop(true);
        //the phases before this one end on idle, which is where the parameter
        //of the tree starts, so the hand over does not snap
        animator.CrossFade(kIdle, kPhaseFade);
        animator.AddBlendClip(kIdle, kBlendThresholds[0]);
        animator.AddBlendClip(kWalking, kBlendThresholds[1]);
        animator.AddBlendClip(kRunning, kBlendThresholds[2]);
        animator.SetBlendParameter(blend_low_);

        LOG_LOG("YBotDemo '{}': blend tree '{}' {} '{}' {} '{}'",
            game_object()->name(), kIdle, kBlendThresholds[0], kWalking, kBlendThresholds[1], kRunning);
        break;
    }
    case Phase::kCrossFade:
        animator.ClearBlendClips();
        animator.SetRootMotion(false);
        animator.SetLoop(true);
        animator.CrossFade(kCrossFadeClips[0], kPhaseFade);
        crossfade_step_ = 0;

        LOG_LOG("YBotDemo '{}': crossfading '{}', '{}' and '{}' every {:.2f}s",
            game_object()->name(), kCrossFadeClips[0], kCrossFadeClips[1], kCrossFadeClips[2], kCrossFadeHold);
        break;
    case Phase::kTurn:
        animator.ClearBlendClips();
        animator.SetLoop(false);
        if (!animator.SetRootMotionBone(kRootMotionBone)) {
            LOG_WARN("YBotDemo '{}': the rig has no bone '{}'", game_object()->name(), kRootMotionBone);
        }

        animator.SetRootMotion(true);
        turn_step_ = 0;
        turn_wrapped_ = false;
        turn_distance_ = 0.0f;
        turn_angle_ = 0.0f;
        animator.CrossFade(kRootMotionClips[turn_step_], kPhaseFade);

        LOG_LOG("YBotDemo '{}': root motion on, playing '{}' as a one shot",
            game_object()->name(), kRootMotionClips[turn_step_]);
        break;
    case Phase::kNone:
        break;
    }
}

void YBotDemo::UpdateBlendTree(Animator& animator) {
    //a ramp that rises and falls again, so the pose slides from idle up to
    //running and back without ever snapping from one clip to another
    float period = mode_ == Mode::kTimeline ? kBlendPhaseTime : blend_period_;
    float phase = std::fmod(phase_time_ / period, 1.0f);
    float ramp = 0.5f - 0.5f * std::cos(phase * kTau);
    animator.SetBlendParameter(blend_low_ + (blend_high_ - blend_low_) * ramp);
}

void YBotDemo::UpdateCrossFade(Animator& animator) {
    size_t step = (size_t)(phase_time_ / kCrossFadeHold);
    if (step == crossfade_step_) {
        return;
    }

    crossfade_step_ = step;

    const size_t count = sizeof(kCrossFadeCycle) / sizeof(kCrossFadeCycle[0]);
    const char* name = kCrossFadeCycle[step % count];

    //the pose is unchanged on the frame the fade starts, the outgoing clip is
    //blended out over the incoming one
    if (animator.CrossFade(name, kCrossFadeDuration)) {
        LOG_LOG("YBotDemo '{}': crossfade to '{}' over {:.2f}s",
            game_object()->name(), name, kCrossFadeDuration);
    }
}

void YBotDemo::UpdateTurn(Animator& animator) {
    //while root motion is on the clip no longer moves the bone it sampled, the
    //game owns that transform and applies what the pose moved by
    auto& tx = transform();
    const Quaternion rig = tx.rotation();

    const Vec3f delta = animator.root_motion_position() * kRigScale;
    if (delta.MagnitudeSq() > 0.0f) {
        //the motion is sampled in the space of the rig, the rotation of the
        //entity is what takes it to the world
        tx.position(tx.position() + rig * delta);
        turn_distance_ += delta.Magnitude();
    }

    //a rotation of the rig is a rotation of the entity around its own up axis
    const Quaternion spin = animator.root_motion_rotation();
    tx.rotation(rig * spin);
    //the angle of the small step of this frame, which is what a controller
    //turning the entity would steer by
    turn_angle_ += 2.0f * std::acos(std::min(1.0f, std::abs(spin.w)));

    if (!turn_wrapped_ && phase_time_ > kTurnPhaseTime - kTurnWrap) {
        //back to idle while the last turn still plays, so the blend tree of the
        //next phase starts from the pose this one ends on
        turn_wrapped_ = animator.CrossFade(kIdle, kTurnWrap);
    }

    if (animator.IsPlaying()) {
        return;
    }

    //the one shot ran out, play the next turn
    const size_t count = sizeof(kRootMotionClips) / sizeof(kRootMotionClips[0]);
    turn_step_ = (turn_step_ + 1) % count;

    LOG_LOG("YBotDemo '{}': '{}' moved the entity {:.2f} m and turned it {:.0f} deg",
        game_object()->name(), kRootMotionClips[(turn_step_ + count - 1) % count],
        turn_distance_, turn_angle_ * math::kRad2Deg);

    turn_distance_ = 0.0f;
    turn_angle_ = 0.0f;

    //a fade between two one shots, so the turns do not snap into each other
    animator.CrossFade(kRootMotionClips[turn_step_], 0.25f);
    LOG_LOG("YBotDemo '{}': playing '{}' as a one shot", game_object()->name(), kRootMotionClips[turn_step_]);
}

}
