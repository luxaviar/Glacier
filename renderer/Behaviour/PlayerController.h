#pragma once

#include <cstdint>
#include "Core/Behaviour.h"
#include "Behaviour/LocomotionBlend.h"
#include "Math/Quat.h"
#include "Math/Vec3.h"

namespace glacier {

class Animator;
class ThirdPersonCamera;

//Drives the Y Bot of the demo scene from the keyboard the way an action game is
//played. WASD is read from the camera - W walks away from the view, A and D
//walk across it - and the character turns towards the way it is going, so the
//mouse stays free to swing the view around it (ThirdPersonCamera). Shift
//switches between walking and running. Idle, walking, the two strafes and
//running are mixed by ComputeLocomotion from the movement measured in the frame
//of the character: a step across it plays a strafe, and the walk cycle takes
//over as the turn completes.
//
//A facing of 0 turns the character along +Z, which is the way the model looks:
//the exporter turns the rig of the Y Bot so that the engine, which reflects the
//scene in Z while importing (aiProcess_MakeLeftHanded), loads it facing the
//forward every camera and controller of the engine uses (Vec3f::forward).
//
//The clips only pose the character: every clip of the Mixamo pack carries the
//motion of its hips (a walk cycle advances 1.65m of it), so root motion is
//switched on to strip that out of the pose and the entity is moved by this
//controller instead.
//
//The clips of the pack are also all authored on one cycle - the right foot lands
//at the start of it and the left one half a cycle later, whichever direction the
//clip walks in (build/probe/phase_probe.py measures it on the asset) - so the
//controller keeps a single phase and puts every clip at that point of its own
//cycle. That is what keeps the feet of a blend together: a diagonal mixes a walk
//and a strafe, and once their cycles have drifted apart the legs work against
//each other. The phase turns with the ground the character covers - one turn of
//it is one stride of the clip - which is also what stops the feet from sliding,
//and it stops when the character stands, so a step left half taken waits there.
//
//What is not part of the locomotion - the jump and the turn in place - is played
//as an action over it. The animator mixes the clips it is given by weight, so
//the action takes the pose by rising to one while the blend is scaled down under
//it; averaging the two instead would leave a leg of the walk standing in an air
//pose. The action is driven by its own time as well, so the code and the pose
//agree on when the feet leave the floor.
//
//Space jumps: the clip of the pack carries the character through its crouch, and
//at the moment the clip stands it on the floor for the last time the controller
//takes the height over and gives it the speed that lifts it by jump_height;
//gravity brings it back to the ground, where the pose of the clip is handed back
//to the blend over the absorb it plays after the touch down. The pack has one
//jump, and it is a one shot: it cannot be started again while the character is
//off the floor.
//
//A character standing where it is that is asked to go somewhere it is not facing
//pivots on the spot with the turn clips of the pack (a quarter turn of 0.9s and
//a turn of about 176 degrees of 1.6s). The clip turns the hips, and root motion
//has taken that rotation out of the pose the nodes are given, so the controller
//turns the entity by the same angle, following the hips of the pose: that is
//what keeps the feet of the clip on the floor while the body comes about.
//Walking a corner is still left to the blend, which is what the strafes are for.
//
//Stripping the root bone of a clip pins the hips where they rest, which would
//leave the feet of a crouch in the air; while the character stands on the floor
//the controller therefore follows the hips of the pose, so a crouch, a landing
//and the bob of a walk cycle all move the character the way the clips of the
//pack were authored.
class PlayerController : public Behaviour {
public:
    //how fast the character swings around to the way it walks; it settles a
    //turn in about a fifth of a second, quick enough to feel like an action
    //game and slow enough to see the turn
    static constexpr float kDefaultTurnSpeed = 14.0f;

    //How far the character has to be turned before it pivots where it stands
    //instead of walking the turn, in degrees. Below this the strafes of the pack
    //carry the turn, which is what they are for; above it a character that is
    //standing still looks like it is skating if it is asked to walk sideways
    //round a corner.
    static constexpr float kDefaultTurnAngle = 65.0f;
    //the angle above which the long turn of the pack is played instead of the
    //quarter turn: the pack comes about in one clip, and half of a turn is too
    //much for the quarter one to leave to the steering
    static constexpr float kHalfTurnAngle = 135.0f;
    //how fast the character may already be going for a pivot, as a share of the
    //walking speed: turning where it stands belongs to a character that is
    //standing, one that is walking walks the turn
    static constexpr float kPivotSpeed = 0.5f;
    //how long the pose of a turn takes to reach the body and to leave it again,
    //and how long an action that is being replaced takes to leave the pose
    static constexpr float kTurnFade = 0.12f;
    static constexpr float kActionFade = 0.1f;
    //the fastest the pose of a turn may turn the character, in degrees a second.
    //The turn clips turn at about a hundred; anything quicker is the blend of the
    //action arriving over the locomotion, which is not a step
    static constexpr float kMaxPoseTurnSpeed = 360.0f;

    //how high the jump of the character lifts it, in metres, and how strongly
    //gravity pulls it back down. Both are the clip: the jump of the pack was
    //authored to lift the feet about half a metre, and the gravity is the one
    //that makes the flight last as long as the clip is off the floor, so the
    //feet leave and meet the ground with the pose (see
    //build/probe/jump_clip_probe.py, which reads the clip out of the glTF)
    static constexpr float kDefaultJumpHeight = 0.55f;
    static constexpr float kDefaultGravity = -12.0f;
    //when the toes of the clip leave the floor, and how long the crouch before
    //it and the absorb after it take to fade into the pose of the character
    static constexpr float kJumpTakeoff = 0.72f;
    static constexpr float kJumpFade = 0.1f;
    static constexpr float kLandFade = 0.25f;

    //how fast the character picks up speed and how fast it sheds it, in m/s²;
    //the speed itself is the one the clips of the pack were measured at
    float accel() const { return accel_; }
    void accel(float v) { accel_ = v < 0.0f ? 0.0f : v; }
    float decel() const { return decel_; }
    void decel(float v) { decel_ = v < 0.0f ? 0.0f : v; }

    //the height of the jump, in metres, and the gravity that ends it, in m/s²
    //(negative is down; a positive value is read as a pull downwards)
    float jump_height() const { return jump_height_; }
    void jump_height(float v) { jump_height_ = v < 0.0f ? 0.0f : v; }
    float gravity() const { return gravity_; }
    void gravity(float v) { gravity_ = v > 0.0f ? -v : v; }
    //where the feet stand: the demo floor is the y = 0 plane of the scene
    float ground() const { return ground_y_; }
    void ground(float v) { ground_y_ = v; }
    //the character is between the take off and the touch down
    bool airborne() const { return jump_phase_ == JumpPhase::kAirborne; }
    //the character is playing the jump, including its crouch and its landing
    bool jumping() const { return jump_phase_ != JumpPhase::kNone; }
    //the character is turning where it stands, including the pose leaving again
    bool turning() const { return turn_phase_ != TurnPhase::kNone; }

    //the camera WASD is measured from; without one the world axes are used
    void SetCamera(ThirdPersonCamera* camera) { camera_ = camera; }
    ThirdPersonCamera* camera() const { return camera_; }

    //how fast the character turns towards the way it goes, in 1/s; 0 turns it
    //on the spot. This is the one knob the feel of the steering lives in
    float turn_speed() const { return turn_speed_; }
    void turn_speed(float v) { turn_speed_ = v < 0.0f ? 0.0f : v; }

    //how far the character has to be turned to pivot where it stands, in degrees
    float turn_angle() const { return turn_angle_; }
    void turn_angle(float v) { turn_angle_ = v < 0.0f ? 0.0f : v; }

    float speed() const { return speed_; }
    //facing of the character in radians, yaw 0 looks along +Z
    float yaw() const { return yaw_; }
    //where in the shared cycle of the locomotion clips the character is
    float phase() const { return phase_; }

    void Update(float dt) override;

private:
    //what the character is doing, only to tell the log when it changes
    enum class State : uint8_t {
        kIdle,
        kWalking,
        kRunning
    };

    //where the one shot of the jump is: the clip crouches first, the controller
    //owns the height in the air, and the pose of the landing is handed back to
    //the blend while the clip plays its absorb
    enum class JumpPhase : uint8_t {
        kNone,
        kWindup,
        kAirborne,
        kLanding
    };

    //where the turn in place is: the clip turns the body, then the pose leaves
    //the blend again while the steering takes the character over
    enum class TurnPhase : uint8_t {
        kNone,
        kTurning,
        kSettling
    };

    void Resolve();
    //turns the facing towards the given yaw the short way round
    void TurnTowards(float target, float dt);
    //sheds or picks up speed towards the target the pose asked for and steps
    void Move(float target_speed, float dt);
    //puts the blend of the locomotion and the action that rides over it on the
    //animator, every frame
    void ApplyPose(const LocomotionBlend& blend, float dt);
    //starts a one shot over the blend, taking over the pose of the one before it
    void BeginAction(const char* clip, float duration);
    //takes the action off the blend, so that a clip left behind by the next one
    //cannot stay in the pose
    void ClearAction();
    //the clip a direction of the blend tree walks with, and how long one cycle
    //of it lasts
    const char* ClipOfDirection(int direction, bool run) const;
    float ClipDuration(const char* name) const;
    //the clip of the pack that turns the character the given way, or null when
    //the asset has no turn to pivot with
    const char* PickTurnClip(bool left, float magnitude) const;
    //starts the one shot of the jump and its crouch
    void BeginJump();
    //carries the character through the crouch, the flight and the landing
    void UpdateJump(const LocomotionBlend& blend, float dt);
    //starts the pivot the keys are asking for
    void BeginTurn(float target_yaw);
    //follows the pose of the turn with the entity, and ends it
    void UpdateTurn(float dt);
    //the time of the one shot, the phase of its clip and the weight it takes
    //the pose with
    void AdvanceAction(float dt, float fade);
    //how far the pose of the clip lifts the body from the rest pose, in metres
    float PoseLift() const;
    //where the pose of the character points, as a yaw in the frame of the rig
    bool PoseYaw(float& yaw) const;
    //the clip the blend of this frame leans on, which the landing names in the log
    static const char* DominantClip(const LocomotionBlend& blend);
    //puts the origin of the character at the given height above the world
    void SetHeight(float height);
    //how far the character is from the camera, -1 without one
    float CameraDistance() const;
    static const char* StateName(State state);

    //what the animator was given last frame, so a weight that did not move is
    //not set again (every set weighs the whole pose)
    struct AppliedWeights {
        float idle = -1.0f;
        float walking = -1.0f;
        float running = -1.0f;
        float left_walk = -1.0f;
        float left_run = -1.0f;
        float right_walk = -1.0f;
        float right_run = -1.0f;
        float action = -1.0f;
        float fading = -1.0f;
    };

    Animator* animator_ = nullptr;
    ThirdPersonCamera* camera_ = nullptr;
    //the rotation the model node carries itself (the armature of the rig turns
    //the model upright and looks along +Z), which every facing is composed with
    Quaternion rig_rotation_ = Quaternion::identity;
    //the scale of the model node, which takes the centimetres of the rig to the
    //metres of the engine
    float rig_scale_ = 0.01f;
    //the bone the clips carry the character on, and where it rests
    size_t hips_bone_ = 0;
    Vec3f rest_hips_ = Vec3f::zero;
    //where the character walks, in the world and in its own frame; both are held
    //while it slows down so the pose settles into idle along the way it went
    Vec3f move_direction_ = Vec3f::forward;
    Vec3f local_direction_ = Vec3f::forward;
    float yaw_ = 0.0f;
    float speed_ = 0.0f;
    float accel_ = 12.0f;
    float decel_ = 16.0f;
    float turn_speed_ = kDefaultTurnSpeed;
    float turn_angle_ = kDefaultTurnAngle;

    //the cycle every clip of the pack is put at, and the ground one turn of it
    //covers for each direction of the blend tree
    float phase_ = 0.0f;
    locomotion::LocomotionCycles cycles_;

    //the action that rides over the blend, its time within its own clip, the
    //phase that clip is put at, and how much of the pose it takes
    const char* action_clip_ = nullptr;
    float action_time_ = 0.0f;
    float action_duration_ = 0.0f;
    float action_phase_ = 0.0f;
    float action_weight_ = 0.0f;
    //the action that is on its way out while a new one takes the pose: a jump
    //can be asked for while the character is pivoting, and the pose of the turn
    //has to leave over the arrival of the jump rather than in a single frame
    const char* fading_clip_ = nullptr;
    float fading_weight_ = 0.0f;

    JumpPhase jump_phase_ = JumpPhase::kNone;
    //an asset without a jump clip keeps the character on the ground
    bool can_jump_ = false;
    //speed of the character up and down, in m/s, while it is in the air
    float vertical_speed_ = 0.0f;
    float jump_height_ = kDefaultJumpHeight;
    float gravity_ = kDefaultGravity;
    float ground_y_ = 0.0f;

    TurnPhase turn_phase_ = TurnPhase::kNone;
    //an asset without a turn clip of its own walks the turn with the strafes
    bool can_turn_ = false;
    //the way the pivot goes: -1 is the left of the character, which is the way
    //the yaw of the engine goes down
    bool turn_left_ = true;
    //the facing the pivot started from, and how far the pose has turned since
    float turn_start_yaw_ = 0.0f;
    float turn_amount_ = 0.0f;
    float turn_pose_yaw_ = 0.0f;
    bool turn_pose_valid_ = false;

    //what was handed to the animator last frame, so weights that did not move
    //are not set again
    AppliedWeights applied_;
    State state_ = State::kIdle;
    //where the walk that is going on now started, for the log
    Vec3f travel_start_;
    float travel_distance_ = 0.0f;
    float travel_time_ = 0.0f;
};

}
