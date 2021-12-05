#pragma once

#include "plane.h"
#include "aabb.h"

namespace glacier {

namespace render {
    class Camera;
}

enum class FrustumCorner : uint8_t {
    kNearBottomLeft = 0,
    kNearTopLeft,
    kNearTopRight,
    kNearBottomRight,
    kFarBottomLeft,
    kFarTopLeft,
    kFarTopRight,
    kFarBottomRight,
    kCount
};

enum class FrustumPlane : uint8_t {
    kNear,
    kFar,
    kLeft,
    kRight,
    kTop,
    kBottom,
    kCount
};

struct Frustum {
    Frustum(const render::Camera& camera);
    Frustum(const render::Camera& camera, float n, float f);
    Frustum(const Vec3f corners[(int)FrustumCorner::kCount]);

    const Plane& GetFrustumPlane(FrustumPlane id) const {
        assert(id != FrustumPlane::kCount);
        return planes[(int)id]; 
    }

    bool Intersect(const AABB& aabb) const;
    bool Intersect(const Vec3f& pos, float radius) const;

    Plane planes[(int)FrustumPlane::kCount]; // the bounding planes
};

}
