#include "CapsuleCollider.h"
#include "Render/Editor/Gizmos.h"

namespace glacier {

CapsuleCollider::CapsuleCollider(float height, float radius) :
    Collider(ShapeCategory::kCapusule),
    radius_(radius),
    half_height_(height / 2.0f),
    //axis_(axis),
    segment_version_(0) 
{
}

CapsuleCollider::CapsuleCollider(float height, float radius, 
    float mass, float friction, float restitution) :
    Collider(ShapeCategory::kCapusule, mass, friction, restitution),
    radius_(radius),
    half_height_(height / 2.0f),
    //axis_(axis),
    segment_version_(0)
{

}

const LineSegment& CapsuleCollider::Segment() {
    if (segment_version_ != transform().version()) {
        Vec3f top(0.0f, half_height_, 0.0f);
        Vec3f buttom(0.0f, -half_height_, 0.0f);

        //top[toUType(axis_)] = half_height_;
        //buttom[toUType(axis_)] = -half_height_;

        segment_.a = transform().ApplyTransform(buttom);
        segment_.b = transform().ApplyTransform(top);

        segment_version_ = transform().version();
    }

    return segment_;
}

Vec3f CapsuleCollider::ClosestPoint(const Vec3f& point) {
    const LineSegment& ls = Segment();
    Vec3f ptOnline = ls.ClosestPoint(point);
    float lensq = ptOnline.DistanceSq(point);
    if (lensq <= radius_ * radius_) {
        return point;
    } else {
        return ptOnline + (point - ptOnline).Scale(radius_);
    }
}

bool CapsuleCollider::Contains(const Vec3f& point) {
    Vec3f ptOnline = Segment().ClosestPoint(point);
    float lensq = ptOnline.DistanceSq(point);
    if (lensq <= radius_ * radius_) {
        return true;
    }

    return false;
}

float CapsuleCollider::Distance(const Vec3f& point) {
    Vec3f closePoint = ClosestPoint(point);
    return (point - closePoint).Magnitude();
}

Vec3f CapsuleCollider::FarthestPoint(const Vec3f& wdir) {
    const LineSegment& ls = Segment();
    Vec3f pt = wdir.Dot(ls.b - ls.a) >= 0 ? ls.b : ls.a;
    return pt + wdir * radius_;
}

// http://iquilezles.org/www/articles/intersectors/intersectors.htm
bool CapsuleCollider::Intersects(const Ray& ray, float max, float& t) {
    const LineSegment& ls = Segment();
    Vec3f u = ls.b - ls.a;
    Vec3f v = ray.direction;
    Vec3f w0 = ray.origin - ls.a;
    Vec3f ro = ray.origin;
    Vec3f pb = ls.b;
    float r = radius_;
    
    float baba = u.Dot(u);
    float bard = u.Dot(v);
    float baoa = u.Dot(w0);
    float rdoa = v.Dot(w0);
    float oaoa = w0.Dot(w0);
    float a = baba - bard * bard;
    float b = baba * rdoa - baoa * bard;
    float c = baba * oaoa - baoa * baoa - r * r * baba;
    float h = b * b - a * c;
    if (h >= 0.0) {
        t = (-b - math::Sqrt(h)) / a;
        float y = baoa + t * bard;
        // body
        if (y > 0.0f && y < baba) {
            if (t > 0.0f && t < max)
                return true;
            else
                return false;
        }
        // caps
        Vec3f oc = (y <= 0.0) ? w0 : ro - pb;
        b = v.Dot(oc);
        c = oc.Dot(oc) - r * r;
        h = b * b - c;
        if (h > 0.0f) {
            t = -b - math::Sqrt(h);
            if (t > 0.0f && t < max)
                return true;
            else
                return false;
        }
    }

    t = -1.0f;
    return false;
}

AABB CapsuleCollider::CalcAABB() {
    const LineSegment& ls = Segment();
    Vec3f d(radius_);
    return AABB(Vec3f::Min(ls.a, ls.b) - d, Vec3f::Max(ls.a, ls.b) + d);
}

//https://www.randygaul.net/2014/08/24/inertia-tensor-capsule/
Matrix3x3 CapsuleCollider::CalcInertiaTensor(float mass) {
    float h = (half_height_ * 2.0f);
    float r2 = radius_ * radius_;
    float cylinder_mass = h / (h + 4.0f / 3.0f * radius_) * mass;
    float semi_sphere_mass = (mass - cylinder_mass) / 2.0f;

    float x = semi_sphere_mass * ((3.0f * radius_ + 2.0f * h) / 8.0f) * h;
    float y = 2.0f / 5.0f * semi_sphere_mass * r2;

    float v = 1.0f / 12.0f * cylinder_mass * (3.0f * r2 + h * h);
    float i = v + 2.0f * x;
    float m = 1.0f / 2.0f * cylinder_mass * r2 + 2.0f * y;

    Vec3f diagonal(i, m, i);
    //diagonal[toUType(axis_)] = m;

    return Matrix3x3::FromDiagonal(diagonal);
}

void CapsuleCollider::OnDrawSelectedGizmos() {
    auto rot = transform().rotation();
    render::Gizmos::Instance()->DrawCapsule(transform().position(), half_height_ * 2.0f, radius_, rot);
}

}

