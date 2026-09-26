#pragma once

#include <cmath>

//The animation of the demo character is a mix of the clips of the Mixamo pack,
//which covers four directions - forward, backward and a strafe to each side -
//and, for each of them, walking and running, with idle sitting in the middle.
//ComputeLocomotion turns the local input of one frame into the weight of every
//clip of that pack plus the speed the character should move at. It is plain
//maths on purpose, so it can be checked without a window (build/probe).

namespace glacier {

//The pack has no "run backwards" clip, the character backs up walking.
constexpr float kWalkSpeed = 1.60f;
constexpr float kRunSpeed = 4.20f;
constexpr float kStrafeWalkSpeed = 1.66f;
constexpr float kStrafeRunSpeed = 4.30f;

//the four directions of the pack in the order the angle walks through them:
//forward, right, backward and left. They were measured at slightly different
//speeds, and the forward walk is what the character backs up with, so the
//backward direction has the walk of the character and no run of its own.
constexpr float kDirectionWalkSpeed[4] = { kWalkSpeed, kStrafeWalkSpeed, kWalkSpeed, kStrafeWalkSpeed };
constexpr float kDirectionRunSpeed[4] = { kRunSpeed, kStrafeRunSpeed, kWalkSpeed, kStrafeRunSpeed };

//Local input of one frame of the character: x is its right, z the way it faces.
struct LocomotionInput {
    //direction of the movement, unit length. The controller holds the direction
    //of the last frame while the character slows down, so the pose settles into
    //idle along the way it was going instead of snapping to another heading
    float forward = 1.0f;
    float strafe = 0.0f;
    //shift is held
    bool run = false;
    //speed of the character right now, in metres per second, which is what
    //decides how much of the pose is idle
    float speed = 0.0f;
};

//Weight of every clip of the pack for one frame. The weights add up to one.
struct LocomotionBlend {
    float idle = 1.0f;
    float forward_walk = 0.0f;
    float forward_run = 0.0f;
    float backward_walk = 0.0f;
    float left_walk = 0.0f;
    float left_run = 0.0f;
    float right_walk = 0.0f;
    float right_run = 0.0f;
    //Which way the shared cycle of the clips runs. Backing up walks the cycle
    //backwards, because a walk cycle played backwards is a walk backwards, and
    //every clip takes the same direction: the clips of the pack are all built
    //on one cycle, so they stay lined up with each other that way.
    float phase_direction = 1.0f;
    //speed the character wants to reach with this input, in metres per second
    float target_speed = 0.0f;
};

//The ground one cycle of every clip of the pack covers, in metres: the clips
//were measured walking at a speed and last as long as they last, so a turn of
//the cycle carries the character that many metres along. The controller fills
//it in from the clips of the asset it was given.
namespace locomotion {

struct LocomotionCycles {
    float walk[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float run[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
};

inline float Clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

//the speed the clip of one direction was measured at, forward (0), right,
//backward and left, walking and running
inline float WalkSpeed(int direction) {
    return kDirectionWalkSpeed[direction & 3];
}

inline float RunSpeed(int direction) {
    return kDirectionRunSpeed[direction & 3];
}

//Walks through the four directions of the pack and fills in the two the
//movement of this frame sits between: forward (0), right, backward and left in
//the order the angle goes through them.
inline void DirectionWeights(float forward, float strafe, float weights[4]) {
    constexpr float kQuarterTurn = 1.57079632679f;  //pi / 2
    constexpr float kFullTurn = 6.28318530718f;     //2 pi

    float angle = std::atan2(strafe, forward);
    if (angle < 0.0f) {
        angle += kFullTurn;
    }

    int first = (int)(angle / kQuarterTurn) % 4;
    float t = angle / kQuarterTurn - (float)first;

    for (int i = 0; i < 4; ++i) {
        weights[i] = 0.0f;
    }

    weights[first] = 1.0f - t;
    weights[(first + 1) % 4] = t;
}

//Splits the current speed into the share of idle, of the walk clip and of the
//run clip: the walk clip is in full at the speed it was measured at, the run
//clip takes over by the speed it was measured at, and idle fills in below the
//walk. A direction the pack has no run for passes its own walk speed as the run
//speed, which holds its walk clip instead of blending towards a run.
inline void SplitBySpeed(float speed, float walk_speed, float run_speed, float& idle, float& walk, float& run) {
    if (speed <= 0.0f || walk_speed <= 0.0f) {
        idle = 1.0f;
        walk = 0.0f;
        run = 0.0f;
        return;
    }

    if (speed < walk_speed) {
        walk = speed / walk_speed;
        idle = 1.0f - walk;
        run = 0.0f;
        return;
    }

    idle = 0.0f;

    float span = run_speed - walk_speed;
    if (span <= 0.001f) {
        walk = 1.0f;
        run = 0.0f;
        return;
    }

    run = Clamp01((speed - walk_speed) / span);
    walk = 1.0f - run;
}

inline float TargetSpeed(float forward, float strafe, bool run) {
    float weights[4];
    DirectionWeights(forward, strafe, weights);

    float speed = 0.0f;
    for (int i = 0; i < 4; ++i) {
        speed += weights[i] * (run ? RunSpeed(i) : WalkSpeed(i));
    }

    return speed;
}

//the ground one cycle of a clip covers: it was measured walking at a speed and
//lasts the time it lasts, so a turn of it carries the character along by the
//speed times the duration
inline float CycleStride(float speed, float duration) {
    return speed * duration;
}

//How far the character travels in one turn of the shared cycle with this input.
//The directions of the pack walk at slightly different speeds and the clips
//last slightly different times, so the stride of a blend is the one of the
//directions it mixes.
inline float BlendedStride(const LocomotionCycles& cycles, float forward, float strafe, bool run) {
    float weights[4];
    DirectionWeights(forward, strafe, weights);

    const float* strides = run ? cycles.run : cycles.walk;

    float stride = 0.0f;
    for (int i = 0; i < 4; ++i) {
        stride += weights[i] * strides[i];
    }

    return stride;
}

//The point of the cycle the clips of the pack are put at. The clips are all
//authored on one cycle - the right foot lands at its start and the left one half
//a cycle later, whichever way the clip walks (build/probe/phase_probe.py
//measures it on the asset) - so one phase drives every one of them and the feet
//of a blend stay together. The phase turns with the ground: `distance` metres
//covered is that much of `stride`, and a character backing up turns it the
//other way.
inline float AdvancePhase(float phase, float distance, float stride) {
    if (stride <= 0.0001f) {
        return phase;
    }

    phase += distance / stride;

    //a cycle has no end, and the phase can be walked both ways, so it wraps
    //into [0, 1) from either side
    phase -= ::floorf(phase);
    return phase;
}

}  // namespace locomotion

inline LocomotionBlend ComputeLocomotion(const LocomotionInput& input) {
    using namespace locomotion;

    LocomotionBlend out;

    float forward = input.forward;
    float strafe = input.strafe;
    float magnitude = std::sqrt(forward * forward + strafe * strafe);

    if (magnitude <= 0.0001f) {
        return out;
    }

    //a direction that is not normalized would move the character faster than the
    //clips walk, so diagonals are scaled back to one
    if (magnitude > 1.0f) {
        forward /= magnitude;
        strafe /= magnitude;
    }

    float weights[4];
    DirectionWeights(forward, strafe, weights);

    //every direction mixes its own pair of clips, because the clips of the pack
    //were measured at slightly different speeds; the weights still add up to
    //one because the three shares of a direction always do
    float idle = 0.0f;
    float walk[4];
    float run[4];
    for (int i = 0; i < 4; ++i) {
        float direction_idle = 0.0f;
        SplitBySpeed(input.speed, WalkSpeed(i), RunSpeed(i), direction_idle, walk[i], run[i]);
        idle += weights[i] * direction_idle;
    }

    //weights[0] forward, [1] right, [2] backward, [3] left. The pack has no run
    //backwards, so the split never hands the backward direction a run share
    out.idle = idle;
    out.forward_walk = weights[0] * walk[0];
    out.forward_run = weights[0] * run[0];
    out.right_walk = weights[1] * walk[1];
    out.right_run = weights[1] * run[1];
    out.backward_walk = weights[2] * walk[2];
    out.left_walk = weights[3] * walk[3];
    out.left_run = weights[3] * run[3];
    out.phase_direction = weights[2] > 0.0f ? -1.0f : 1.0f;

    float walk_speed = 0.0f;
    float run_speed = 0.0f;
    for (int i = 0; i < 4; ++i) {
        walk_speed += weights[i] * WalkSpeed(i);
        run_speed += weights[i] * RunSpeed(i);
    }

    out.target_speed = input.run ? run_speed : walk_speed;

    return out;
}

}
