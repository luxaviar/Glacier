#pragma once

#include "collider.h"
#include "geometry/linesegment.h"
#include "math/axis.h"

namespace glacier {

class CylinderCollider : public Collider {
public:
    CylinderCollider(float height, float radius);

    CylinderCollider(float height, float radius, 
        float mass, float friction, float restitution);

    const LineSegment& Segment();
    Vec3f ClosestPoint(const Vec3f& point) override;
    bool Contains(const Vec3f& point) override;
    float Distance(const Vec3f& point) override;
    Vec3f FarthestPoint(const Vec3f& wdir) override;
    bool Intersects(const Ray& ray, float max, float& t) override;
    Matrix3x3 CalcInertiaTensor(float mass) override;

    void OnDrawSelectedGizmos() override;

protected:
    AABB CalcAABB() override;

protected:
    float radius_;
    float half_height_;
    //Axis axis_;

    uint32_t segment_version_;
    LineSegment segment_;

};

}
