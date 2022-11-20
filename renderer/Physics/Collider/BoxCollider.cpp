#include "BoxCollider.h"
#include "render/editor/gizmos.h"
#include "Lux/Lux.h"

namespace glacier {

LUX_IMPL(BoxCollider, BoxCollider)
LUX_CTOR(BoxCollider, const Vec3f&)
LUX_IMPL_END

BoxCollider::BoxCollider(const Vec3f& extents) : 
    Collider(ShapeCategory::kOBB),
    rot_version_(0),
    extents_(extents) 
{
}

BoxCollider::BoxCollider(const Vec3f& extents, float mass, float friction, float restitution) :
    Collider(ShapeCategory::kOBB, mass, friction, restitution),
    rot_version_(0),
    extents_(extents)
{

}

const Matrix3x3& BoxCollider::Axis() {
    if (rot_version_ != transform().version()) {
        rotation_ = transform().rotation().Inverted().ToMatrix();
        rot_version_ = transform().version();
    }

    return rotation_;
}

bool BoxCollider::Contains(const Vec3f& point) {
    Vec3f d = point - position();
    const Matrix3x3& axis = Axis();
    return math::Abs(d.Dot(axis[0])) <= extents_[0] &&
        math::Abs(d.Dot(axis[1])) <= extents_[1] &&
        math::Abs(d.Dot(axis[2])) <= extents_[2];
}

bool BoxCollider::Intersects(const Ray& ray, float max, float& t) {
    AABB aabb(Vec3f(-extents_.x, -extents_.y, -extents_.z), extents_);
    const Matrix4x4& m = transform().WorldToLocalMatrix();
    Ray localRay(m.MultiplyPoint3X4(ray.origin), m.MultiplyVector(ray.direction).Normalized());

    return aabb.Intersects(localRay, max, t);
}

float BoxCollider::Distance(const Vec3f& point) {
    return ClosestPoint(point).Distance(point);
}

AABB BoxCollider::CalcAABB() {
    const Matrix3x3& axis = Axis();
    Vec3f ext = axis.r0.Abs() * extents_.x + axis.r1.Abs() * extents_.y + axis.r2.Abs() * extents_.z;
    return AABB(position() - ext, position() + ext);
}

Vec3f BoxCollider::ClosestPoint(const Vec3f& point) {
    Vec3f closestPoint = position();
    Vec3f d = point - closestPoint;
    const Matrix3x3& axis = Axis();
    for (int i = 0; i < 3; ++i) {
        closestPoint += axis[i] * math::Clamp(d.Dot(axis[i]), -extents_[i], extents_[i]);
    }

    return closestPoint;
}

Vec3f BoxCollider::FarthestPoint(const Vec3f& wdir) {
    Vec3f pt = position();
    const Matrix3x3& axis = Axis();
    pt += (axis.r0 * (wdir.Dot(axis.r0) >= 0.0f ? extents_.x : -extents_.x));
    pt += (axis.r1 * (wdir.Dot(axis.r1) >= 0.0f ? extents_.y : -extents_.y));
    pt += (axis.r2 * (wdir.Dot(axis.r2) >= 0.0f ? extents_.z : -extents_.z));

    return pt;
}

Matrix3x3 BoxCollider::CalcInertiaTensor(float mass) {
    float v = 1.0f / 3.0f * mass;
    float w2 = extents_.x * extents_.x;
    float h2 = extents_.y * extents_.y;
    float d2 = extents_.z * extents_.z;
    return Matrix3x3(
        v * (h2 + d2), 0.0f, 0.0f,
        0.0f, v * (w2 + h2), 0.0f,
        0.0f, 0.0f, v * (w2 + d2)
    );
}

void BoxCollider::OnDrawSelectedGizmos() {
    render::Gizmos::Instance()->DrawCube(transform().position(), extents_, transform().rotation());
}

}

