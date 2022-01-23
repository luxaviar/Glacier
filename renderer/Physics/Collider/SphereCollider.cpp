#include "SphereCollider.h"
#include "Render/Editor/Gizmos.h"
#include "Geometry/LineSegment.h"

namespace glacier {

SphereCollider::SphereCollider(float radius) : 
    Collider(ShapeCategory::kSphere),
    radius_(radius) 
{
}

SphereCollider::SphereCollider(float radius, float mass, float friction, float restitution) :
    Collider(ShapeCategory::kSphere, mass, friction, restitution),
    radius_(radius)
{

}

bool SphereCollider::Contains(const Vec3f& point) {
    return (position() - point).Magnitude() <= radius_;
}

bool SphereCollider::Intersects(const Ray& ray, float max, float& t) {
    Vec3f oc = position() - ray.origin;
    float b = oc.Dot(ray.direction);
    float c = oc.Dot(oc) - radius_ * radius_;
    float h = b * b - c;

    if (h < 0.0f) {
        t = -1.0f;
        return false;
    }

    h = math::Sqrt(h);
    t = -b - h;
    if (t < 0.0f) {
        t = 0.0f;
    }

    if (max > 0.0f && t > max) return false;

    return true;
}

// https://wickedengine.net/2020/04/26/capsule-collision-detection
bool SphereCollider::Intersect(const Triangle& tri, Vector3* closest_point) {
    Vector3 center = position(); // sphere center
    Vector3 N = (tri.b - tri.a).Cross(tri.c - tri.a).Normalized();
    float dist = (center - tri.a).Dot(N); // signed distance between sphere and plane
    if (dist < -radius_ || dist > radius_)
        return false; // no intersection

    Vector3 point0 = center - N * dist; // projected sphere center on triangle plane
    // Now determine whether point0 is inside all triangle edges: 
    Vector3 c0 = (point0 - tri.a).Cross(tri.b - tri.a);
    Vector3 c1 = (point0 - tri.b).Cross(tri.c - tri.b);
    Vector3 c2 = (point0 - tri.c).Cross(tri.a - tri.c);
    bool inside = c0.Dot(N) <= 0 && c1.Dot(N) <= 0 && c2.Dot(N) <= 0;

    float radiussq = radius_ * radius_; // sphere radius squared

    // Edge 1:
    LineSegment edge1(tri.a, tri.b);
    Vector3 point1 = edge1.ClosestPoint(center);
    Vector3 v1 = center - point1;
    float distsq1 = v1.Dot(v1);
    bool intersects = distsq1 < radiussq;

    // Edge 2:
    LineSegment edge2(tri.b, tri.c);
    Vector3 point2 = edge2.ClosestPoint(center);
    Vector3 v2 = center - point2;
    float distsq2 = v2.Dot(v2);
    intersects |= distsq2 < radiussq;

    // Edge 3:
    LineSegment edge3(tri.c, tri.a);
    Vector3 point3 = edge3.ClosestPoint(center);
    Vector3 v3 = center - point3;
    float distsq3 = v3.Dot(v3);
    intersects |= distsq3 < radiussq;

    if (inside || intersects) {
        Vector3 best_point = point0;
        Vector3 intersection_vec;

        if (inside) {
            intersection_vec = center - point0;
        } else {
            Vector3 d = center - point1;
            float best_distsq = d.Dot(d);
            best_point = point1;
            intersection_vec = d;

            d = center - point2;
            float distsq = d.Dot(d);
            if (distsq < best_distsq) {
                distsq = best_distsq;
                best_point = point2;
                intersection_vec = d;
            }

            d = center - point3;
            distsq = d.Dot(d);
            if (distsq < best_distsq) {
                distsq = best_distsq;
                best_point = point3;
                intersection_vec = d;
            }
        }

        float len = intersection_vec.Magnitude();  // vector3 length calculation: 
        Vector3 penetration_normal = intersection_vec / len;  // normalize
        float penetration_depth = radius_ - len; // radius = sphere radius
        return true; // intersection success
    }

    return false;
}

float SphereCollider::Distance(const Vec3f& point) {
    float d = (position() - point).Magnitude();
    if (d <= radius_) return 0;

    return d - radius_;
}

AABB SphereCollider::CalcAABB() {
    Vec3f pos = position();
    Vec3f ext(radius_, radius_, radius_);
    return AABB(pos - ext, pos + ext);
}

Vec3f SphereCollider::ClosestPoint(const Vec3f& point) {
    Vec3f pos = position();
    float d = pos.Distance(point);
    float t = (d >= radius_ ? radius_ : d);
    return pos + (point - pos) * (t / d);
}

Vec3f SphereCollider::FarthestPoint(const Vec3f& wdir) {
    return position() + wdir * radius_;
}

Matrix3x3 SphereCollider::CalcInertiaTensor(float mass) {
    float v = 2.0f / 5.0f * mass * radius_ * radius_;
    return Matrix3x3(
        v, 0.0f, 0.0f,
        0.0f, v, 0.0f,
        0.0f, 0.0f, v
    );
}

void SphereCollider::OnDrawSelectedGizmos() {
    render::Gizmos::Instance()->DrawSphere(transform().position(), radius_);
    render::Gizmos::Instance()->DrawCube(bounds().Center(), bounds().Extent());
}

}

