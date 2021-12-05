#pragma once

#include "ray.h"
#include "Math/Util.h"

namespace glacier {

struct Triangle {
    static void Barycentric(const Vec3f& p, const Vec3f& a, const Vec3f& b, const Vec3f& c, float& u, float& v, float& w);

    Triangle(const Vec3f& a, const Vec3f& b, const Vec3f& c);

    void Barycentric(const Vec3f& p, float& u, float& v, float& w) const;
    bool InPositiveSpace(const Vec3f& point);
    bool Contains(const Vec3f& point, float threshold = math::kEpsilon);
    float ClosestEdgeDist(const Vec3f& point);
    //bool Contains(const Vec3f& point);
    bool Intersects(const Ray& ray, float& t) const;
    bool Intersects(const Ray& ray, float max, float& t) const;
    Vec3f ClosestPoint(const Vec3f& p) const;
    Vec3f Normal(); 
    Vec3f CircumSphereCenter();

    //CW order
    Vec3f a;
    Vec3f b;
    Vec3f c;

private:
    Vec3f normal_;
};

}
