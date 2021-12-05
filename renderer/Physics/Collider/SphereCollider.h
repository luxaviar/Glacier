#pragma once

#include "collider.h"

namespace glacier {

class SphereCollider : public Collider {
public:
    SphereCollider(float radius);
    SphereCollider(float radius, float mass, float friction, float restitution);

    float radius() const { return radius_; }

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
    float radius_;
};

}
