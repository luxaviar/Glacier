#include "CylinderCollider.h"
#include <assert.h>
#include "Render/Editor/Gizmos.h"

namespace glacier {

CylinderCollider::CylinderCollider(float height, float radius) :
    Collider(ShapeCategory::kCylinder),
    radius_(radius),
    half_height_(height / 2.0f),
    //axis_(axis),
    segment_version_(0)
{
}

CylinderCollider::CylinderCollider(float height, float radius,
    float mass, float friction, float restitution) :
    Collider(ShapeCategory::kCylinder, mass, friction, restitution),
    radius_(radius),
    half_height_(height / 2.0f),
    //axis_(axis),
    segment_version_(0)
{

}

const LineSegment& CylinderCollider::Segment() {
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

//https://liris.cnrs.fr/Documents/Liris-1297.pdf
Vec3f CylinderCollider::ClosestPoint(const Vec3f& point) {
    const LineSegment& ls = Segment();
    float len = ls.Length();
    Vec3f u = (ls.b - ls.a) / len;
    Vec3f c = ls.Center();
    float x = (point - c).Dot(u);
    Vec3f h = c + x * u;
    float n2 = (point - c).MagnitudeSq();
    float y2 = n2 - x * x;
    if (math::Abs(x) < len / 2) {
        if (y2 < radius_* radius_) {
            return point;
        } else {
            return h + (point - h).Normalized() * sqrt(y2);
        }
    } else {
        if (y2 < radius_ * radius_) {
            Vec3f d = x > 0 ? (ls.b - h) : (ls.a - h);
            return point + d;
        } else {
            Vec3f s = x > 0 ? ls.b : ls.a;
            return s + (point - h).Normalized() * radius_;
        }
    }
}

// https://www.flipcode.com/archives/Fast_Point-In-Cylinder_Test.shtml
bool CylinderCollider::Contains(const Vec3f& point) {
    const LineSegment& ls = Segment();
    Vec3f pa = point - ls.a;
    Vec3f ba = ls.b - ls.a;
    float lensq = ba.MagnitudeSq();
    float project = pa.Dot(ba);// / len);
    if (project < 0.0f || project > lensq) {
        return false;
    }

    float distsq = pa.Dot(pa) - project * project / lensq;
    if (distsq <= radius_ * radius_) {
        return true;
    }

    return false;
}

float CylinderCollider::Distance(const Vec3f& point) {
    const LineSegment& ls = Segment();
    float len = ls.Length();
    Vec3f u = (ls.b - ls.a) / len;
    Vec3f c = ls.Center();
    float x = (point - c).Dot(u);
    //Vec3f h = c + x * u;
    float n2 = (point - c).MagnitudeSq();
    float y2 = n2 - x * x;
    if (math::Abs(x) < len / 2) {
        if (y2 < radius_* radius_) {
            return 0;
        } else {
            return sqrt(y2) - radius_;
        }
    } else {
        if (y2 < radius_ * radius_) {
            return math::Abs(x) - len / 2;
        } else {
            return sqrt(pow(sqrt(y2) - radius_, 2.0f) + pow(math::Abs(x) - len / 2.0f, 2.0f));
        }
    }
}

Vec3f CylinderCollider::FarthestPoint(const Vec3f& wdir) {
    const LineSegment& ls = Segment();
    Vec3f center = ls.Center();
    Vec3f ldir = (ls.b - ls.a);
    ldir.Normalize();

    float ldw = ldir.Dot(wdir);
    Vec3f wproject = wdir - (ldw * ldir);
    float wlen = wproject.Magnitude();
    Vec3f pt;
    if (wlen > math::kEpsilon) {
        pt = center + math::Sign(ldw) * half_height_ * ldir + radius_ * (wproject / wlen);
    } else {
        pt = center + math::Sign(ldw) * half_height_ * ldir;
    }

    return pt;
}

// http://iquilezles.org/www/articles/intersectors/intersectors.htm
bool CylinderCollider::Intersects(const Ray& ray, float max, float& t) {
    const LineSegment& ls = Segment();
    Vec3f pb = ls.b;
    Vec3f pa = ls.a;
    Vec3f ro = ray.origin;
    Vec3f rd = ray.direction;
    float ra = radius_;

    Vec3f ca = pb - pa;
    Vec3f oc = ro - pa;
    float caca = ca.Dot(ca);
    float card = ca.Dot(rd);
    float caoc = ca.Dot(oc);
    float a = caca - card * card;
    float b = caca * oc.Dot(rd) - caoc * card;
    float c = caca * oc.Dot(oc) - caoc * caoc - ra * ra * caca;
    float h = b * b - a * c;
    if (h < 0.0f) {
        t = -1.0f;
        return false;
    }

    h = math::Sqrt(h);
    t = (-b - h) / a;
    // body
    float y = caoc + t * card;
    if (y > 0.0f && y < caca) {
        return true;
    }
    // caps
    t = (((y < 0.0f) ? 0.0f : caca) - caoc) / card;
    if (math::Abs(b + a * t) < h) {
        return true;
    }

    return false;
}

AABB CylinderCollider::CalcAABB() {
    const LineSegment& ls = Segment();
    Vec3f a = ls.b - ls.a;
    Vec3f e = (Vec3f::one - a * a / a.Dot(a)).Sqrt() * radius_;
    Vec3f min = Vec3f::Min(ls.a - e, ls.b - e);
    Vec3f max = Vec3f::Max(ls.a + e, ls.b + e);
    return AABB(min, max);
}

Matrix3x3 CylinderCollider::CalcInertiaTensor(float mass) {
    float h = (half_height_ * 2.0f);
    float r2 = radius_ * radius_;
    float v = 1.0f / 12.0f * mass * (3.0f * r2 + h * h);
    float m = 1.0f / 2.0f * mass * r2;

    Vec3f diagonal(v, m, v);
    //diagonal[toUType(axis_)] = m;

    return Matrix3x3::FromDiagonal(diagonal);
}

void CylinderCollider::OnDrawSelectedGizmos() {
    auto rot = transform().rotation();
    render::Gizmos::Instance()->DrawCylinder(transform().position(), half_height_ * 2.0f, radius_, rot);
}

}

