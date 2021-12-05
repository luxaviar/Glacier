#pragma once

#include "collider.h"

namespace glacier {

class BoxCollider : public Collider {
public:
    BoxCollider(const Vec3f& extents);
    BoxCollider(const Vec3f& extents, float mass, float friction, float restitution);

    const Matrix3x3& Axis();
    bool Contains(const Vec3f& point) override;
    bool Intersects(const Ray& ray, float max, float& t) override;
    float Distance(const Vec3f& point) override;
    Vec3f ClosestPoint(const Vec3f& point) override;
    Vec3f FarthestPoint(const Vec3f& wdir) override;
    Matrix3x3 CalcInertiaTensor(float mass) override;
    void OnDrawSelectedGizmos() override;

protected:
    AABB CalcAABB() override;

private:
    uint32_t rot_version_;
    Vec3f extents_;
    // world -> local rotation, row 0 -> X axis, row 1 -> Y axis, row 2 -> Z axis
    Matrix3x3 rotation_;
};

}
