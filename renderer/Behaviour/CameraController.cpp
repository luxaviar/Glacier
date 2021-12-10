#include "CameraController.h"
#include "Math/Util.h"
#include "Render/Camera.h"
#include "Input/Input.h"

namespace glacier {

CameraController::CameraController(render::Camera* cam) noexcept :
    camera_(cam)
{
    Reset();
}

void CameraController::Reset() {
    auto dir = camera_->forward();

    pitch_ = (float)::asin(-dir.y);
    yaw_ = (float)::atan2(dir.x, dir.z);
}

void CameraController::Rotate(float dx, float dy) noexcept
{
    yaw_ = math::WrapAngle(yaw_ + dx * kRotationSpeed);
    pitch_ = math::Clamp(pitch_ + dy * kRotationSpeed, 0.995f * -math::kPI / 2.0f, 0.995f * math::kPI / 2.0f);

    auto rot = Quaternion::FromEuler(pitch_, yaw_, 0.0f);
    camera_->rotation(rot);
}

void CameraController::Translate(const Vec3f& translation) noexcept
{
    auto rot = camera_->rotation();
    auto pos = camera_->position();

    auto trans = rot * translation;
    pos += (trans * kTravelSpeed);

    camera_->position(pos);
}

void CameraController::Update(float dt) {
    if (!Input::IsRelativeMode()) {
        float delta = Input::GetMouseWheelDelta();
        if (delta == 0.0f) return;
        float fov = camera_->fov();
        fov -= delta;
        fov = math::Clamp(fov, 30, 120);
        camera_->fov(fov);

        return;
    }

    auto& state = Input::GetKeyState();
    if (state.W) {
        Translate({ 0.0f,0.0f,dt });
    }
    if (state.A)  {
        Translate({ -dt,0.0f,0.0f });
    }
    if (state.S) {
        Translate({ 0.0f,0.0f,-dt });
    }
    if (state.D) {
        Translate({ dt,0.0f,0.0f });
    }
    if (state.R) {
        Translate({ 0.0f,dt,0.0f });
    }
    if (state.F) {
        Translate({ 0.0f,-dt,0.0f });
    }

    int deltax, deltay;
    Input::ReadRawDelta(deltax, deltay);

    if (deltax != 0 || deltay != 0) {
        Rotate((float)deltax, (float)deltay);
    }
}

}
