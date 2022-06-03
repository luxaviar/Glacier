#include "Frustum.h"
#include "Render/Camera.h"
#include "Render/Mesh/Geometry.h"

namespace glacier {

Frustum::Frustum(const render::Camera& camera) : Frustum(camera, camera.nearz(), camera.farz()) {

}

Frustum::Frustum(const render::Camera& camera, float n, float f) {
    assert(n < f && n > 0);

    Vec3f corners[(int)FrustumCorner::kCount];
    camera.FetchFrustumCorners(corners, n, f);
    render::geometry::FetchFrustumPlanes(planes.data(), corners);
}

Frustum::Frustum(const Vec3f corners[(int)FrustumCorner::kCount]) {
    render::geometry::FetchFrustumPlanes(planes.data(), corners);
}

bool Frustum::Intersect(const AABB& aabb) const {
    return aabb.Intersects(planes.data(), (int)FrustumPlane::kCount);
}

bool Frustum::Intersect(const Vec3f& pos, float radius) const {
    for (int i = 0; i < (int)FrustumPlane::kCount; ++i)
    {
        auto& p = planes[i];
        if (p.DistanceFromPoint(pos) + radius < 0.0f)
            return false;
    }
    return true;
}

}
