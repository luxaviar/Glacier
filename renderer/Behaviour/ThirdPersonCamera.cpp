#include "Behaviour/ThirdPersonCamera.h"

#include <cmath>

#include "Core/GameObject.h"
#include "Core/Transform.h"
#include "Input/Input.h"
#include "Lux/Lux.h"
#include "Math/Util.h"
#include "Render/Camera.h"

namespace glacier {

LUX_IMPL(ThirdPersonCamera, ThirdPersonCamera)
LUX_CTOR(ThirdPersonCamera, render::Camera*)
LUX_FUNC(ThirdPersonCamera, SetTarget)
LUX_FUNC(ThirdPersonCamera, SetAngle)
LUX_FUNC(ThirdPersonCamera, SyncFromCamera)
LUX_FUNC(ThirdPersonCamera, Rotate)
LUX_PROP_FUNC_GET(ThirdPersonCamera, yaw, yaw)
LUX_PROP_FUNC_GET(ThirdPersonCamera, pitch, pitch)
LUX_PROP_FUNC(ThirdPersonCamera, height)
LUX_PROP_FUNC(ThirdPersonCamera, distance)
LUX_PROP_FUNC(ThirdPersonCamera, follow_speed)
LUX_IMPL_END

ThirdPersonCamera::ThirdPersonCamera(render::Camera* cam) noexcept :
    camera_(cam)
{
}

void ThirdPersonCamera::distance(float v) {
    distance_ = math::Clamp(v, kMinDistance, kMaxDistance);
}

void ThirdPersonCamera::SetAngle(float yaw, float pitch) {
    yaw_ = math::WrapAngle(yaw);
    pitch_ = math::Clamp(pitch, kMinPitch, kMaxPitch);
    //the scene decided where the camera looks, the first frame must not overwrite it
    sync_pending_ = false;
}

void ThirdPersonCamera::SyncFromCamera() {
    if (!camera_) {
        return;
    }

    //the same convention CameraController uses: yaw 0 looks along +Z and a
    //positive pitch looks up, so a scene that aimed the camera with LookAt can
    //be read back into the orbit angles
    auto dir = camera_->forward().Normalized();
    yaw_ = (float)::atan2(dir.x, dir.z);
    pitch_ = math::Clamp((float)::asin(math::Clamp(-dir.y, -1.0f, 1.0f)), kMinPitch, kMaxPitch);
    sync_pending_ = false;
}

void ThirdPersonCamera::Rotate(float dx, float dy) noexcept {
    yaw_ = math::WrapAngle(yaw_ + dx * kRotationSpeed);
    pitch_ = math::Clamp(pitch_ + dy * kRotationSpeed, kMinPitch, kMaxPitch);
}

void ThirdPersonCamera::Update(float dt) {
    if (!camera_) {
        return;
    }

    if (sync_pending_) {
        SyncFromCamera();
    }

    //the wheel moves the camera in and out in either mode
    float wheel = Input::GetMouseWheelDelta();
    if (wheel != 0.0f) {
        distance(distance_ - wheel * kZoomSpeed);
    }

    //while the right button is held the app is in relative mode and the mouse
    //turns the orbit: the view swings around the character, which is free to
    //walk in another direction, the way an action game aims
    if (Input::IsRelativeMode()) {
        int deltax = 0;
        int deltay = 0;
        Input::ReadRawDelta(deltax, deltay);

        if (deltax != 0 || deltay != 0) {
            Rotate((float)deltax, (float)deltay);
        }
    }
}

void ThirdPersonCamera::LateUpdate(float dt) {
    if (!camera_) {
        return;
    }

    auto rotation = Quaternion::FromEuler(pitch_ * math::kRad2Deg, yaw_ * math::kRad2Deg, 0.0f);
    auto forward = rotation * Vec3f::forward;

    auto position = camera_->position();

    if (target_) {
        //the camera sits the distance behind the pivot, along the direction it looks
        auto pivot = target_->transform().position() + Vec3f(0.0f, height_, 0.0f);
        auto wanted = pivot - forward * distance_;
        //a low orbit must not put the camera under the floor
        wanted.y = math::Max(wanted.y, kMinHeight);

        if (follow_speed_ > 0.0f && dt > 0.0f) {
            //the frame time of the first frame carries the load of the scene, and
            //an exponential step keeps the follow stable for any of them
            float t = 1.0f - (float)::exp(-follow_speed_ * dt);
            position = Vec3f::Lerp(position, wanted, t);
        }
        else {
            position = wanted;
        }
    }

    camera_->position(position);
    camera_->rotation(rotation);
}

}
