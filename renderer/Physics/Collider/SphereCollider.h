#pragma once

#include "collider.h"
#include "Geometry/Triangle.h"

namespace glacier {

class SphereCollider : public Collider {
public:
    SphereCollider(float radius);
    SphereCollider(float radius, float mass, float friction, float restitution);

    float radius() const { return radius_; }

    bool Contains(const Vec3f& point) override;
    bool Intersects(const Ray& ray, float max, float& t) override;
    bool Intersect(const Triangle& tri, Vector3* closest_point = nullptr);

    float Distance(const Vec3f& point) override;
    Vec3f ClosestPoint(const Vec3f& point) override;
    Vec3f FarthestPoint(const Vec3f& wdir) override;
    Matrix3x3 CalcInertiaTensor(float mass) override;

    void OnDrawSelectedGizmos() override;

protected:
    AABB CalcAABB() override;

private:
    float radius_;
};

}
