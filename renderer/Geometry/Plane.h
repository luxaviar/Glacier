#pragma once

#include "Math/Vec3.h"
#include "Math/Vec4.h"
#include "Math/Mat4.h"
#include "Ray.h"

namespace glacier {

struct Plane {
    Plane() {}
    Plane(const Vec3f& plane_normal, float distance_from_origin) : d(distance_from_origin), normal(plane_normal) {}
    Plane(const Vec3f& plane_normal, const Vec3f& point_on_plane);
    Plane(const Vec3f& a, const Vec3f& b, const Vec3f& c);
    Plane(float a, float b, float c, float d) : d(d), normal(a, b, c) {}
    Plane(const Vec4f& plane) : d(plane.w), normal(plane.x, plane.y, plane.z) {}
    Plane(const Plane& other) : d(other.d), normal(other.normal) {}

    // Returns the point on the plane closest to the origin
    Vec3f PointOnPlane() const { return normal * d; }

    // Distance from 3D point
    float DistanceFromPoint(const Vec3f& point) const {
        return point.Dot(normal) - d;
    }

    bool Intersect(Ray& ray, float max, float& t);

    //// Most efficient way to transform a plane.  (Involves one quaternion-vector rotation and one dot product.)
    //friend Plane operator* (const Transform& xform, const Plane& plane) {
    //    Vec3f normal = xform.rotation() * plane.normal;
    //    float distance = plane.d - xform.position().Dot(normal);
    //    return Plane(normal, distance);
    //}

    // Less efficient way to transform a plane (but handles affine transformations.)
    friend Plane operator* ( const Matrix4x4& mat, const Plane& plane ) {
        Vec4f v = (mat.InvertedOrthonormal().Transposed() * Vec4f(plane.normal, plane.d));
        return Plane(Vec3f(v.x, v.y, v.z), v.w);
    }

    float d;
    Vec3f normal;
};

}
