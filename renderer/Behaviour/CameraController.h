#pragma once

#include <string>
#include "Math/Vec3.h"
#include "core/behaviour.h"

namespace glacier {

namespace render {
    class Camera;
}

class CameraController : public Behaviour
{
public:
    static constexpr float kTravelSpeed = 10.0f;
    static constexpr float kRotationSpeed = 0.004f;

    CameraController(render::Camera* cam) noexcept;

    void Reset();
    void Rotate(float dx, float dy) noexcept;
    void Translate(const Vec3f& translation) noexcept;

    void Update(float dt) override;

private:
    render::Camera* camera_;
    float pitch_;
    float yaw_;
};

}
