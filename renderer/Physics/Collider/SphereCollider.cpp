#include "SphereCollider.h"
#include "Render/Editor/Gizmos.h"

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

