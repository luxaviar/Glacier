#include "Behaviour/PlayerController.h"

#include <algorithm>
#include <cmath>

#include "Animation/Animator.h"
#include "Animation/AnimationClip.h"
#include "Behaviour/ThirdPersonCamera.h"
#include "Common/Log.h"
#include "Core/GameObject.h"
#include "Core/Transform.h"
#include "Input/Input.h"
#include "Lux/Lux.h"
#include "Math/Util.h"

namespace glacier {

LUX_IMPL(PlayerController, PlayerController)
LUX_CTOR(PlayerController)
LUX_FUNC(PlayerController, SetCamera)
LUX_PROP_FUNC(PlayerController, accel)
LUX_PROP_FUNC(PlayerController, decel)
LUX_PROP_FUNC(PlayerController, jump_height)
LUX_PROP_FUNC(PlayerController, gravity)
LUX_PROP_FUNC(PlayerController, ground)
LUX_PROP_FUNC(PlayerController, turn_speed)
LUX_PROP_FUNC(PlayerController, turn_angle)
LUX_PROP_FUNC_GET(PlayerController, speed, speed)
LUX_PROP_FUNC_GET(PlayerController, yaw, yaw)
LUX_PROP_FUNC_GET(PlayerController, phase, phase)
LUX_PROP_FUNC_GET(PlayerController, airborne, airborne)
LUX_PROP_FUNC_GET(PlayerController, jumping, jumping)
LUX_PROP_FUNC_GET(PlayerController, turning, turning)
LUX_IMPL_END

namespace {

//the clips of the Y Bot pack, see tools/mixamo_to_gltf.py; the files the pack
//was built from are named after the directions of the strafes
constexpr const char* kIdle = "idle";
constexpr const char* kWalking = "walking";
constexpr const char* kRunning = "running";
constexpr const char* kLeftStrafeWalking = "left strafe walking";
constexpr const char* kLeftStrafe = "left strafe";
constexpr const char* kRightStrafeWalking = "right strafe walking";
constexpr const char* kRightStrafe = "right strafe";
//the one shot space plays, and the four turns the character pivots with
constexpr const char* kJump = "jump";
constexpr const char* kLeftTurn = "left turn";
constexpr const char* kLeftTurn90 = "left turn 90";
constexpr const char* kRightTurn = "right turn";
constexpr const char* kRightTurn90 = "right turn 90";

//the clips of the pack in the order the direction weights are in: forward (0),
//right, backward and left. Backing up walks the walk cycle backwards and the
//walk cycle has no run of its own, so the backward direction is in both tables
constexpr const char* kDirectionWalkClip[4] = { kWalking, kRightStrafeWalking, kWalking, kLeftStrafeWalking };
constexpr const char* kDirectionRunClip[4] = { kRunning, kRightStrafe, kWalking, kLeftStrafe };

//every clip an asset of the pack is expected to have, for the log
constexpr const char* kPackClips[] = {
    kIdle, kWalking, kRunning,
    kLeftStrafeWalking, kLeftStrafe, kRightStrafeWalking, kRightStrafe,
};

//the clips keep the motion of the character on the hips
constexpr const char* kRootMotionBone = "mixamorig:Hips";

//a weight has to move by this much before it is handed to the animator
constexpr float kWeightEpsilon = 0.002f;
//above this speed the character counts as moving
constexpr float kMovingSpeed = 0.05f;

}

const char* PlayerController::StateName(State state) {
    switch (state) {
    case State::kWalking:
        return "walking";
    case State::kRunning:
        return "running";
    default:
        return "idle";
    }
}

const char* PlayerController::ClipOfDirection(int direction, bool run) const {
    return run ? kDirectionRunClip[direction & 3] : kDirectionWalkClip[direction & 3];
}

float PlayerController::ClipDuration(const char* name) const {
    auto clip = animator_->GetClip(name);

    //an asset that is missing a clip of the pack still gets driven: a cycle of
    //the phase then lasts a second, which is close enough to keep the pose from
    //standing still
    return clip ? clip->duration() : 1.0f;
}

void PlayerController::Resolve() {
    animator_ = game_object() ? game_object()->GetComponent<Animator>() : nullptr;
    if (!animator_) {
        LOG_WARN("PlayerController '{}': no animator",
            game_object() ? game_object()->name().c_str() : "<none>");
        return;
    }

    //the clips only pose the character, the controller moves the entity: with
    //root motion on the hips no longer write the motion of the clip into the
    //pose and the character stays where the controller puts it
    if (animator_->SetRootMotionBone(kRootMotionBone)) {
        //the bone the clip carries the character on is also the one the height
        //and the turn of the pose are read from
        hips_bone_ = animator_->root_motion_bone();

        auto skeleton = animator_->skeleton();
        if (skeleton && hips_bone_ < skeleton->bone_count()) {
            rest_hips_ = skeleton->bone(hips_bone_).rest_position;
        }
    }
    else {
        LOG_WARN("PlayerController '{}': the rig has no bone '{}'",
            game_object()->name().c_str(), kRootMotionBone);
    }

    animator_->SetRootMotion(true);
    animator_->SetLoop(true);
    animator_->SetSpeed(1.0f);

    //the jump is a one shot of its own length, whatever the other clips do
    can_jump_ = animator_->GetClip(kJump) != nullptr && ClipDuration(kJump) > 0.0f;

    if (!can_jump_) {
        //an asset without the one shot leaves the character on the floor
        LOG_WARN("PlayerController '{}': the asset has no '{}' clip, the character cannot jump",
            game_object()->name().c_str(), kJump);
    }

    //The ground one cycle of a clip covers, which is what the shared phase of
    //the locomotion turns with: a cycle of 'walking' lasts 1.03s and the clip
    //was measured walking at 1.60 m/s, so the feet of it are authored for 1.65m
    //of ground. The clips of the pack are all built on that one cycle, so the
    //phase of every one of them is the same number.
    for (int i = 0; i < 4; ++i) {
        cycles_.walk[i] = locomotion::CycleStride(locomotion::WalkSpeed(i), ClipDuration(ClipOfDirection(i, false)));
        cycles_.run[i] = locomotion::CycleStride(locomotion::RunSpeed(i), ClipDuration(ClipOfDirection(i, true)));
    }

    int missing = 0;
    for (const char* name : kPackClips) {
        if (!animator_->GetClip(name)) {
            ++missing;
        }
    }

    if (missing > 0) {
        LOG_WARN("PlayerController '{}': the asset is missing {} of the {} clips of the pack, "
            "the phase falls back to a cycle a second",
            game_object()->name().c_str(), missing, (int)(sizeof(kPackClips) / sizeof(kPackClips[0])));
    }

    //a character pivots where it stands with the turn clips of the pack, and one
    //that has none walks the turn like any other
    can_turn_ = PickTurnClip(true, 180.0f) != nullptr;

    if (!can_turn_) {
        LOG_WARN("PlayerController '{}': the asset has no turn clip, the character walks its turns",
            game_object()->name().c_str());
    }

    //the model node carries a rotation of its own, the armature that turns the
    //rig upright and puts its front on +Z, and a scale that takes the
    //centimetres of the rig to the metres of the engine; every facing is
    //composed with the one and every height with the other
    rig_rotation_ = transform().rotation();
    rig_scale_ = transform().local_scale().y;
    //the scene opens with the camera behind the character, and the character
    //starts with its back to it, the way the view of an action game opens
    yaw_ = camera_ ? camera_->yaw() : 0.0f;

    if (!camera_) {
        LOG_WARN("PlayerController '{}': no camera, WASD is read from the world axes",
            game_object()->name().c_str());
    }

    LOG_LOG("PlayerController '{}': {} clips, facing {:.0f} deg, root motion on '{}', turn speed {:.1f}, "
        "one cycle covers {:.2f}m walking and {:.2f}m running",
        game_object()->name().c_str(), animator_->clip_count(), yaw_ * math::kRad2Deg,
        kRootMotionBone, turn_speed_,
        locomotion::CycleStride(locomotion::WalkSpeed(0), ClipDuration(kWalking)),
        locomotion::CycleStride(locomotion::RunSpeed(0), ClipDuration(kRunning)));
}

void PlayerController::Update(float dt) {
    if (!animator_) {
        Resolve();
        if (!animator_) {
            return;
        }
    }

    //the frame that loaded the scene carries its load time, and a hitch must
    //not teleport the character
    dt = std::min(dt, 0.1f);

    auto& keys = Input::GetKeyState();
    float forward = (keys.W ? 1.0f : 0.0f) - (keys.S ? 1.0f : 0.0f);
    float strafe = (keys.D ? 1.0f : 0.0f) - (keys.A ? 1.0f : 0.0f);
    bool run = keys.LeftShift || keys.RightShift;

    //WASD is read from the view, not from the character: W walks the way the
    //camera looks, which is away from it, and D walks towards the right of the
    //screen, so the keys keep meaning the same thing while the mouse turns the
    //view around
    auto view_forward = camera_ ? camera_->FlatForward() : Vec3f::forward;
    auto view_right = camera_ ? camera_->FlatRight() : Vec3f::right;
    auto move = view_forward * forward + view_right * strafe;

    float magnitude = move.Magnitude();
    bool moving = magnitude > 0.0001f;

    if (moving) {
        //a diagonal counts as one direction, so it is not faster than walking
        //straight
        move = move / magnitude;
        move_direction_ = move;

        float target = (float)::atan2(move.x, move.z);

        //a character standing where it is that is asked to go somewhere it is not
        //facing pivots on the spot: at those angles the strafes of the pack are
        //not a step sideways any more, they are a shuffle on the spot
        if (can_turn_ && turn_phase_ == TurnPhase::kNone && jump_phase_ == JumpPhase::kNone &&
            speed_ <= kWalkSpeed * kPivotSpeed &&
            std::abs(math::WrapAngle(target - yaw_)) > turn_angle_ * math::kDeg2Rad) {
            BeginTurn(target);
        }

        //the pose of a pivot is what turns the character, and steering the facing
        //at the same time would fight it; the steering takes over again as the
        //pose of the turn leaves the blend
        if (turn_phase_ != TurnPhase::kTurning) {
            //the character turns towards the way it goes instead of turning with
            //the camera, so the view can be swung around while it walks
            TurnTowards(target, dt);
        }
    }

    //the pose is picked from the movement as the character sees it: walking the
    //way it faces plays the walk cycle, a step across plays a strafe, and the
    //shares move as the turn completes
    auto facing = Quaternion::FromEuler(0.0f, yaw_ * math::kRad2Deg, 0.0f);

    if (moving) {
        local_direction_ = Vec3f(move.Dot(facing * Vec3f::right), 0.0f,
            move.Dot(facing * Vec3f::forward));
    }

    LocomotionInput input;
    input.forward = local_direction_.z;
    input.strafe = local_direction_.x;
    input.run = run;

    //the pose of a pivot has its feet planted, so the character stands while it
    //is being turned: walking through it would drag the feet over the floor
    bool pivoting = turn_phase_ == TurnPhase::kTurning;

    //the pack was measured at the speed of its own clips, so the speed the
    //character wants is the one of the direction it is walking in; without
    //input it wants to stand still and the direction above only settles the
    //pose into idle
    Move(moving && !pivoting ? locomotion::TargetSpeed(input.forward, input.strafe, run) : 0.0f, dt);

    //the weights are picked from the speed of this frame
    input.speed = speed_;
    auto blend = ComputeLocomotion(input);

    //The phase every clip of the pack is put at turns with the ground the
    //character covers, one turn of it being one step. That is what keeps the
    //feet of a blend together - the phases of two clips cannot drift apart if
    //there is only one of them - and it is tied to the distance instead of to
    //the clock, so the feet of a clip stay where the pack put them and the
    //character does not slide. Standing still stops it, which is what keeps a
    //step that was left half taken.
    float stride = locomotion::BlendedStride(cycles_, input.forward, input.strafe, run);
    phase_ = locomotion::AdvancePhase(phase_, blend.phase_direction * speed_ * dt, stride);

    //space jumps. The pack has one jump and it is a one shot, so it cannot be
    //started again in the air; a landing that is still being blended away has
    //the feet on the floor, though, and jumping out of it starts the one shot
    //over
    if (can_jump_ && Input::GetJustKeyDownState().Space &&
        (jump_phase_ == JumpPhase::kNone || jump_phase_ == JumpPhase::kLanding)) {
        //a jump takes the pose over from a pivot: the character is asked to
        //leave the floor, and the turn it was making can be finished from where
        //it got to once it is back on it
        turn_phase_ = TurnPhase::kNone;

        BeginJump();
    }

    if (turn_phase_ != TurnPhase::kNone) {
        UpdateTurn(dt);
    }

    if (jump_phase_ != JumpPhase::kNone) {
        UpdateJump(blend, dt);
    }

    //the blend of the locomotion goes on the animator every frame, whatever the
    //action over it is doing: it keeps running under the action, so the pose the
    //action leaves behind is the one the character went on with
    ApplyPose(blend, dt);

    //The animator pins the hips where they rest, so a pose that crouches leaves
    //the feet in the air (a leg that folds gets shorter under a hip that does
    //not move). Following the hips with the whole character puts them back on
    //the floor, which is what the clips of the pack were authored around: the
    //crouch of a jump, the bend of a turn and the bob of a walk cycle all belong
    //to the character, not just to the pose. In the air the height belongs to
    //the code that threw the character up there.
    if (jump_phase_ != JumpPhase::kAirborne) {
        SetHeight(ground_y_ + PoseLift());
    }

    //what the character is doing, and how far it went the last time it moved;
    //the demo is meant to be watched, so it says what it does
    auto position = transform().position();
    State state = State::kIdle;
    if (speed_ > kMovingSpeed) {
        state = speed_ > (kWalkSpeed + kRunSpeed) * 0.5f ? State::kRunning : State::kWalking;
    }

    if (state != state_ && state != State::kIdle) {
        //the distance to the camera is what a reader of the log needs to tell a
        //walk away from the view from a walk into it
        LOG_LOG("PlayerController '{}': {} at {:.2f} m/s, facing {:.0f} deg, {:.2f} m from the camera",
            game_object()->name().c_str(), StateName(state), speed_,
            yaw_ * math::kRad2Deg, CameraDistance());
    }
    else if (state == State::kIdle && state_ != State::kIdle && travel_distance_ > 0.05f) {
        LOG_LOG("PlayerController '{}': stopped after {:.2f} m in {:.2f} s, moved ({:.2f}, {:.2f}), {:.2f} m from the camera",
            game_object()->name().c_str(), travel_distance_, travel_time_,
            position.x - travel_start_.x, position.z - travel_start_.z, CameraDistance());
    }

    if (state == State::kIdle) {
        travel_distance_ = 0.0f;
        travel_time_ = 0.0f;
    }
    else {
        if (state_ == State::kIdle) {
            travel_start_ = position;
            travel_time_ = 0.0f;
        }

        travel_time_ += dt;
        travel_distance_ = (position - travel_start_).Magnitude();
    }

    state_ = state;
}

void PlayerController::TurnTowards(float target, float dt) {
    //the shortest way round, so a turn never takes the long way
    float delta = math::WrapAngle(target - yaw_);

    if (turn_speed_ <= 0.0f || dt <= 0.0f) {
        yaw_ = math::WrapAngle(target);
        return;
    }

    //an exponential step settles the turn without ever overshooting it, and it
    //keeps its feel whatever the frame time is
    yaw_ = math::WrapAngle(yaw_ + delta * (1.0f - (float)::exp(-turn_speed_ * dt)));
}

void PlayerController::Move(float target_speed, float dt) {
    //without input the target is 0 and the character slows down along the way it
    //was going; the direction stays so the pose settles into idle instead of
    //turning
    float rate = target_speed > speed_ ? accel_ : decel_;

    if (rate <= 0.0f) {
        speed_ = target_speed;
    }
    else if (target_speed > speed_) {
        speed_ = std::min(target_speed, speed_ + rate * dt);
    }
    else {
        speed_ = std::max(target_speed, speed_ - rate * dt);
    }

    auto& tx = transform();

    if (speed_ > 0.0f) {
        //the character walks the way the keys point, even while it is still
        //turning towards them
        tx.position(tx.position() + move_direction_ * (speed_ * dt));
    }

    //the facing goes on through the rotation of the rig
    auto facing = Quaternion::FromEuler(0.0f, yaw_ * math::kRad2Deg, 0.0f);
    tx.rotation(facing * rig_rotation_);
}

void PlayerController::ApplyPose(const LocomotionBlend& blend, float dt) {
    auto set_weight = [this](const char* clip, float weight, float& applied) {
        if (std::abs(weight - applied) < kWeightEpsilon) {
            return;
        }

        applied = weight;
        animator_->SetWeight(clip, weight);
    };

    //An action that rides over the blend takes the pose as it rises, so the
    //blend is scaled down under it. What the animator mixes is one average of
    //every clip that is playing, and half a walk inside the pose of a jump is
    //half a leg standing on nothing.
    float base = 1.0f - action_weight_ - fading_weight_;
    if (base < 0.0f) {
        base = 0.0f;
    }

    set_weight(kIdle, blend.idle * base, applied_.idle);
    //forward and backward share the walk and run cycles of the pack
    set_weight(kWalking, (blend.forward_walk + blend.backward_walk) * base, applied_.walking);
    set_weight(kRunning, blend.forward_run * base, applied_.running);
    set_weight(kLeftStrafeWalking, blend.left_walk * base, applied_.left_walk);
    set_weight(kLeftStrafe, blend.left_run * base, applied_.left_run);
    set_weight(kRightStrafeWalking, blend.right_walk * base, applied_.right_walk);
    set_weight(kRightStrafe, blend.right_run * base, applied_.right_run);

    if (action_clip_) {
        if (action_weight_ > 0.0f) {
            set_weight(action_clip_, action_weight_, applied_.action);
        }
        else {
            //the one shot is over and the blend has the pose back
            ClearAction();
        }
    }

    //the one shot that is being replaced leaves the pose as the new one arrives
    if (fading_clip_) {
        fading_weight_ -= dt / kActionFade;

        if (fading_weight_ > 0.0f) {
            set_weight(fading_clip_, fading_weight_, applied_.fading);
        }
        else {
            set_weight(fading_clip_, 0.0f, applied_.fading);
            fading_clip_ = nullptr;
            fading_weight_ = 0.0f;
            applied_.fading = -1.0f;
        }
    }

    //Every clip of the pack is put at the same point of its own cycle, which is
    //what keeps the feet of a blend together (see the class comment). Only the
    //clips that take part in the blend are worth it: putting a clip at a phase
    //weighs the whole pose again.
    auto set_phase = [this](const char* clip, float weight) {
        if (weight > 0.0f) {
            animator_->SetClipPhase(clip, phase_);
        }
    };

    set_phase(kWalking, blend.forward_walk + blend.backward_walk);
    set_phase(kRunning, blend.forward_run);
    set_phase(kLeftStrafeWalking, blend.left_walk);
    set_phase(kLeftStrafe, blend.left_run);
    set_phase(kRightStrafeWalking, blend.right_walk);
    set_phase(kRightStrafe, blend.right_run);

    if (action_clip_ && action_weight_ > 0.0f) {
        //the one shot is driven by its own time as well, so the code and the
        //pose agree on when the feet of the clip leave the floor
        animator_->SetClipPhase(action_clip_, action_phase_);
    }
}

void PlayerController::BeginAction(const char* clip, float duration) {
    //an action that was already on its way out cannot be left behind in the pose
    if (fading_clip_) {
        if (fading_weight_ > 0.0f) {
            animator_->SetWeight(fading_clip_, 0.0f);
        }

        fading_clip_ = nullptr;
        fading_weight_ = 0.0f;
        applied_.fading = -1.0f;
    }

    if (action_clip_) {
        if (action_weight_ > 0.0f && action_clip_ != clip) {
            //two one shots can meet - a jump asked for while the character is
            //pivoting, say - and the pose of the one that is leaving fades out
            //under the one that is arriving
            fading_clip_ = action_clip_;
            fading_weight_ = action_weight_;
        }
        else {
            //the same clip started again, or one that had already left the pose
            ClearAction();
        }
    }

    action_clip_ = clip;
    action_duration_ = duration;
    action_time_ = 0.0f;
    action_phase_ = 0.0f;
    action_weight_ = 0.0f;
    applied_.action = -1.0f;
}

void PlayerController::ClearAction() {
    if (action_clip_ && applied_.action > 0.0f) {
        animator_->SetWeight(action_clip_, 0.0f);
    }

    action_clip_ = nullptr;
    action_time_ = 0.0f;
    action_duration_ = 0.0f;
    action_phase_ = 0.0f;
    action_weight_ = 0.0f;
    applied_.action = -1.0f;
}

void PlayerController::AdvanceAction(float dt, float fade) {
    action_time_ += dt;

    if (action_duration_ > 0.0f) {
        action_phase_ = std::min(action_time_ / action_duration_, 1.0f);
    }

    if (fade > 0.0f) {
        action_weight_ = std::min(action_weight_ + dt / fade, 1.0f);
    }
}

void PlayerController::BeginJump() {
    //the one shot starts from its first frame: the crouch belongs to the take off
    BeginAction(kJump, ClipDuration(kJump));

    jump_phase_ = JumpPhase::kWindup;
    vertical_speed_ = 0.0f;

    LOG_LOG("PlayerController '{}': jumping, {} m up at {:.1f} m/s², heading {:.1f} m/s at {:.0f} deg",
        game_object()->name().c_str(), jump_height_, gravity_, speed_, yaw_ * math::kRad2Deg);
}

void PlayerController::UpdateJump(const LocomotionBlend& blend, float dt) {
    switch (jump_phase_) {
    case JumpPhase::kWindup:
        //the crouch of the clip: the feet are still on the floor, so the
        //character follows the hips down while it winds up
        AdvanceAction(dt, kJumpFade);

        if (action_time_ >= kJumpTakeoff) {
            //The clip is about to stand the character on the floor for the last
            //time before the feet leave it, so the code takes the height over
            //and gives it the speed that lifts it by jump_height; it reaches
            //the top and comes back down to the ground in the time the clip
            //spends off the floor, which is how the pose keeps landing on the
            //floor the controller lands on.
            jump_phase_ = JumpPhase::kAirborne;
            vertical_speed_ = (float)::sqrt(2.0f * -gravity_ * jump_height_);

            LOG_LOG("PlayerController '{}': take off at {:.2f} m/s from {:.2f} m",
                game_object()->name().c_str(), vertical_speed_,
                transform().position().y - ground_y_);
        }
        break;

    case JumpPhase::kAirborne: {
        //the one shot holds the whole pose while the character is off the floor
        AdvanceAction(dt, kJumpFade);

        float previous_speed = vertical_speed_;
        vertical_speed_ += gravity_ * dt;

        auto& tx = transform();
        auto position = tx.position();
        position.y += vertical_speed_ * dt;

        if (previous_speed > 0.0f && vertical_speed_ <= 0.0f) {
            LOG_LOG("PlayerController '{}': the top of the jump is {:.2f} m above the ground",
                game_object()->name().c_str(), position.y - ground_y_);
        }

        //the character is coming down onto the floor; the crouch it took off
        //from is below it, so only a fall can end the flight
        if (vertical_speed_ <= 0.0f && position.y <= ground_y_) {
            //the touch down: the clip plays its absorb from here, and the pose
            //of the clip is handed back to the blend of the locomotion over it,
            //so the character keeps the momentum it jumped with
            position.y = ground_y_;
            vertical_speed_ = 0.0f;
            jump_phase_ = JumpPhase::kLanding;

            LOG_LOG("PlayerController '{}': landing into '{}' at {:.1f} m/s, {:.0f} deg, {:.2f} m from the camera",
                game_object()->name().c_str(), DominantClip(blend), speed_,
                yaw_ * math::kRad2Deg, CameraDistance());
        }

        tx.position(position);
        break;
    }

    case JumpPhase::kLanding:
        //the absorb of the clip bends the legs and lowers the hips; the pose of
        //the clip is on its way out of the blend, and the character follows it
        //out of the jump the way it followed the crouch into it
        AdvanceAction(dt, 0.0f);
        action_weight_ -= dt / kLandFade;

        if (action_weight_ <= 0.0f) {
            //the one shot has left the blend, which poses the character again
            action_weight_ = 0.0f;
            jump_phase_ = JumpPhase::kNone;

            LOG_LOG("PlayerController '{}': the landing is over, the character is on the ground at {:.2f} m/s",
                game_object()->name().c_str(), speed_);
        }
        break;

    case JumpPhase::kNone:
        break;
    }
}

void PlayerController::BeginTurn(float target_yaw) {
    float delta = math::WrapAngle(target_yaw - yaw_);
    float magnitude = std::abs(delta) * math::kRad2Deg;

    //the yaw of the character goes down when it turns towards its own left,
    //which is the way the turns of the pack are named
    bool left = delta < 0.0f;

    const char* clip = PickTurnClip(left, magnitude);
    if (!clip) {
        //an asset without the turns of the pack walks the turn like any other
        return;
    }

    BeginAction(clip, ClipDuration(clip));

    turn_phase_ = TurnPhase::kTurning;
    turn_left_ = left;
    turn_start_yaw_ = yaw_;
    turn_amount_ = 0.0f;
    turn_pose_valid_ = false;

    LOG_LOG("PlayerController '{}': pivoting {} with '{}', {:.0f} deg to turn at {:.1f} m/s",
        game_object()->name().c_str(), left ? "left" : "right", clip, magnitude, speed_);
}

const char* PlayerController::PickTurnClip(bool left, float magnitude) const {
    auto has = [this](const char* name) {
        return animator_->GetClip(name) != nullptr && ClipDuration(name) > 0.0f;
    };

    //the pack turns a quarter in 0.9s and comes about in 1.6s; whichever of the
    //two is closer to what the keys are asking for is played, and what is left
    //of the angle after it is walked off by the steering
    if (magnitude > kHalfTurnAngle) {
        if (left && has(kLeftTurn)) {
            return kLeftTurn;
        }
        if (!left && has(kRightTurn)) {
            return kRightTurn;
        }
    }

    if (left && has(kLeftTurn90)) {
        return kLeftTurn90;
    }
    if (!left && has(kRightTurn90)) {
        return kRightTurn90;
    }

    //an asset with only the long turn still gets to pivot
    if (left && has(kLeftTurn)) {
        return kLeftTurn;
    }
    if (!left && has(kRightTurn)) {
        return kRightTurn;
    }

    return nullptr;
}

void PlayerController::UpdateTurn(float dt) {
    if (turn_phase_ == TurnPhase::kTurning) {
        AdvanceAction(dt, kTurnFade);

        //The turn of the clip is on the hips, and root motion has taken the
        //rotation of the root bone out of the pose the nodes are given, so the
        //turn has to be put back on the entity by hand: the character turns by
        //as much as the hips of the pose turned, which is what keeps the feet
        //of the clip on the floor while the body comes about.
        float pose_yaw = 0.0f;
        //the turn is counted from the pose the action holds, not from the blend
        //it is arriving over: the little the hips of the walk may be off by is
        //not a turn the character made
        if (action_weight_ >= 0.5f && PoseYaw(pose_yaw)) {
            if (!turn_pose_valid_) {
                turn_pose_yaw_ = pose_yaw;
                turn_pose_valid_ = true;
            }
            else {
                float step = math::WrapAngle(pose_yaw - turn_pose_yaw_);
                turn_pose_yaw_ = pose_yaw;

                //a turn only ever goes one way: the pose wobbles a little while
                //the action fades in over the blend, and a jump of it is not a
                //step the character took
                float limit = kMaxPoseTurnSpeed * math::kDeg2Rad * dt;
                turn_amount_ += math::Clamp(turn_left_ ? -step : step, 0.0f, limit);
                yaw_ = math::WrapAngle(turn_start_yaw_ + (turn_left_ ? -turn_amount_ : turn_amount_));
            }
        }

        //the pose stands on its feet for the whole of the clip, and the last of
        //it is the character settling where it arrived: the frame is handed back
        //to the blend over that, so the two of them never pull the feet apart
        if (action_time_ >= action_duration_ - kTurnFade) {
            turn_phase_ = TurnPhase::kSettling;

            LOG_LOG("PlayerController '{}': pivoted {:.0f} deg {} in {:.2f} s into facing {:.0f} deg, {:.2f} m from the camera",
                game_object()->name().c_str(), turn_amount_ * math::kRad2Deg,
                turn_left_ ? "to the left" : "to the right", action_time_,
                yaw_ * math::kRad2Deg, CameraDistance());
        }
    }
    else if (turn_phase_ == TurnPhase::kSettling) {
        //the clip plays out while its pose leaves the blend
        AdvanceAction(dt, 0.0f);
        action_weight_ -= dt / kTurnFade;

        if (action_weight_ <= 0.0f) {
            action_weight_ = 0.0f;
            turn_phase_ = TurnPhase::kNone;
        }
    }
}

float PlayerController::PoseLift() const {
    Vec3f hips;
    Quaternion hips_rotation;
    Vec3f hips_scale;

    if (!animator_->bone_pose(hips_bone_, hips, hips_rotation, hips_scale)) {
        return 0.0f;
    }

    //the hips hang under the armature of the rig, whose scale is what takes the
    //centimetres the clip was authored in to the metres of the engine
    return (transform().rotation() * ((hips - rest_hips_) * rig_scale_)).y;
}

bool PlayerController::PoseYaw(float& yaw) const {
    Vec3f hips;
    Quaternion hips_rotation;
    Vec3f hips_scale;

    if (!animator_->bone_pose(hips_bone_, hips, hips_rotation, hips_scale)) {
        return false;
    }

    //the hips of a Mixamo rig look along their own +Y, which the armature of the
    //rig turns into the forward of the character, so where the hips of the pose
    //point is where the character of the pose points. The rig is composed in
    //rather than guessed at: its rotation is what stands the model up.
    auto forward = (rig_rotation_ * hips_rotation) * Vec3f::up;
    float length = ::sqrtf(forward.x * forward.x + forward.z * forward.z);
    if (length < 0.0001f) {
        return false;
    }

    yaw = (float)::atan2(forward.x / length, forward.z / length);
    return true;
}

const char* PlayerController::DominantClip(const LocomotionBlend& blend) {
    //forward and backward share the two cycles of the pack, the other two
    //directions have a clip of their own
    const float weights[] = {
        blend.idle,
        blend.forward_walk + blend.backward_walk,
        blend.forward_run,
        blend.left_walk,
        blend.left_run,
        blend.right_walk,
        blend.right_run,
    };
    const char* names[] = {
        kIdle,
        kWalking,
        kRunning,
        kLeftStrafeWalking,
        kLeftStrafe,
        kRightStrafeWalking,
        kRightStrafe,
    };

    size_t best = 0;
    for (size_t i = 1; i < sizeof(weights) / sizeof(weights[0]); ++i) {
        if (weights[i] > weights[best]) {
            best = i;
        }
    }

    return names[best];
}

void PlayerController::SetHeight(float height) {
    auto& tx = transform();
    auto position = tx.position();

    if (position.y != height) {
        position.y = height;
        tx.position(position);
    }
}

float PlayerController::CameraDistance() const {
    if (!camera_) {
        return -1.0f;
    }

    return transform().position().Distance(camera_->transform().position());
}

}
