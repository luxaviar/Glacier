#pragma once

#include <cmath>
#include "Core/Behaviour.h"
#include "Math/Vec3.h"

namespace glacier {

class GameObject;

namespace render {
    class Camera;
}

//Orbit camera of the demo, the view of an action game: it sits at a distance
//from a target and the mouse swings it around that target without turning it.
//A character controller reads where the view points (FlatForward/FlatRight) to
//read WASD from the camera, so pushing forward walks away from the view, the
//way an action game plays. While the right mouse button is held the app puts
//the mouse into relative mode and the mouse steers the orbit (yaw and pitch)
//instead of moving a cursor; the wheel changes how far the camera sits.
class ThirdPersonCamera : public Behaviour {
public:
    //radians per mouse unit, the feel of CameraController
    static constexpr float kRotationSpeed = 0.004f;
    //distance the wheel moves the camera by, per notch
    static constexpr float kZoomSpeed = 0.5f;
    static constexpr float kMinDistance = 1.0f;
    static constexpr float kMaxDistance = 20.0f;
    //how far the orbit may rise above the target and how low it may look up
    //from below it, in radians: a little short of straight up and straight down,
    //which is where an orbit that keeps its own up vector turns over
    static constexpr float kMinPitch = -1.45f;
    static constexpr float kMaxPitch = 1.45f;
    //a low orbit must not put the camera under the floor
    static constexpr float kMinHeight = 0.35f;
    //smoothing of the follow, 0 snaps the camera where it belongs
    static constexpr float kDefaultFollowSpeed = 10.0f;

    explicit ThirdPersonCamera(render::Camera* cam) noexcept;

    //the object the camera orbits; without one it only looks where it is told to
    void SetTarget(GameObject* target) { target_ = target; }
    GameObject* target() const { return target_; }

    //how high above the origin of the target the camera aims and how far it sits
    float height() const { return height_; }
    void height(float v) { height_ = v; }
    float distance() const { return distance_; }
    void distance(float v);

    //where the camera looks at, both in radians: yaw 0 looks along +Z, and the
    //pitch is the elevation of the orbit, positive lifting the camera above the
    //target so it looks down at it
    float yaw() const { return yaw_; }
    float pitch() const { return pitch_; }

    //The view projected on the floor and the right of the screen, which is what
    //a controller measures WASD from: with yaw 0 the forward is +Z and the
    //right is +X, so forward walks away from a camera sitting behind the target
    //and right walks towards the right of the screen.
    Vec3f FlatForward() const { return Vec3f(::sinf(yaw_), 0.0f, ::cosf(yaw_)); }
    Vec3f FlatRight() const { return Vec3f(::cosf(yaw_), 0.0f, -::sinf(yaw_)); }

    void SetAngle(float yaw, float pitch);

    float follow_speed() const { return follow_speed_; }
    void follow_speed(float v) { follow_speed_ = v; }

    //takes the angles from where the camera currently points, which lets a scene
    //place the camera and aim it with LookAt instead of setting the angles by hand
    void SyncFromCamera();

    void Rotate(float dx, float dy) noexcept;

    //aims the camera: the mouse turns the orbit and the wheel changes the
    //distance. Behaviours that read WASD from the view take the angles of this
    //frame, so the camera has to update before them
    void Update(float dt) override;
    //runs after every behaviour moved its object, so the camera follows the place
    //the target ends the frame at instead of the place it started it at
    void LateUpdate(float dt) override;

private:
    render::Camera* camera_ = nullptr;
    GameObject* target_ = nullptr;

    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    float height_ = 1.15f;
    float distance_ = 4.5f;
    float follow_speed_ = kDefaultFollowSpeed;
    //the first update starts from the angles the scene set the camera up with
    bool sync_pending_ = true;
};

}
