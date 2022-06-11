#pragma once

#include <string>
#include <functional>
#include "Math/Vec3.h"
#include "Math/Quat.h"
#include "Math/Mat4.h"
#include "Render/Base/Enums.h"
#include "Geometry/Frustum.h"
#include "Core/Transform.h"
#include "Common/List.h"

namespace glacier {
namespace render {

class Renderable;

using CullFilter = std::function<bool(Renderable*)>;

class Camera : public Component {
public:
    static Camera* Create(CameraType type);

    Camera(CameraType type = CameraType::kPersp) noexcept;
    //Orth: float width, float height, float near = 0.5f, float far = 100.0f) noexcept;

    void LookAt(const Vec3f& target, const Vec3f& up = {0.0f, 1.0f, 0.0f});

    //in world space
    void FetchFrustumCorners(Vec3f corners[(int)FrustumCorner::kCount], float n, float f) const;
    void FetchFrustumCorners(Vec3f corners[(int)FrustumCorner::kCount]) const;

    void Cull(const List<Renderable>& objects, 
        std::vector<Renderable*>& result, const CullFilter& filter = {}) const;

    void Cull(const std::vector<Renderable*>& objects, 
        std::vector<Renderable*>& result, const CullFilter& filter = {}) const;

    Vec3f ViewCenter() const;

    Vec3f CameraToWorld(const Vec3f& pos) const;
    Vec3f WorldToCamera(const Vec3f& pos) const;

    Vec3f forward() const;
    Matrix4x4 view() const noexcept;

    Matrix4x4 projection() const noexcept;
    Matrix4x4 projection_reversez() const noexcept;

    Vec3f position() const noexcept { return transform().position(); }
    void position(const Vec3f& pos) noexcept { transform().position(pos); }

    Quaternion rotation() const noexcept { return transform().rotation(); }
    void rotation(const Quaternion& rot) { transform().rotation(rot); }

    CameraType type() const { return type_; }
    void type(CameraType type) { type_ = type; }

    float fov() const { return param_[0]; }
    void fov(float v) { param_[0] = v; }

    float scale() const { return param_[4]; }
    void scale(float v) { param_[4] = v; }

    float aspect() const { return param_[1]; }
    void aspect(float v) { param_[1] = v; }

    float width() const { return param_[0]; }
    void width(float v) { param_[0] = v; }

    float height() const { return param_[1]; }
    void height(float v) { param_[1] = v; }

    //avoid near/far macro name
    float nearz() const { return param_[2]; }
    void nearz(float v) { param_[2] = v; }

    float farz() const { return param_[3]; }
    void farz(float v) { param_[3] = v; }

    void DrawInspector() override;
    void OnDrawSelectedGizmos() override;

private:
    CameraType type_ = CameraType::kPersp;
    //Persp: fov/aspect/near/far
    //Orhto: width/height/near/far/scale
    float param_[5] = {60.0f, 16.0f / 10.0f, 0.3f, 500.0f, 5.0f};
};

}
}
