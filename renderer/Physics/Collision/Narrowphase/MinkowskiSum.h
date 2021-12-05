#pragma once

#include "stdint.h"
#include "geometry/aabb.h"
//#include "unico/geometry/shape.h"

namespace glacier {

class Collider;

namespace physics {

struct SupportVert {
    static uint32_t id_counter_;

    //world space
    Vec3f point;
    Vec3f pointA;
    Vec3f pointB;
    uint32_t id;

    SupportVert();
    SupportVert(const Vec3f& point_, const Vec3f& pointA_, const Vec3f& pointB_);
};

struct MinkowskiSum {
    static Vec3f StartDir(Collider* a, Collider* b);
    static SupportVert PackSupportPoint(Collider* a, Collider* b, const Vec3f& wdir);

    static Vec3f StartDir(const AABB& a, Collider* b);
    static SupportVert PackSupportPoint(const AABB& a, Collider* b, const Vec3f& wdir);
};

struct Simplex {
    // 6 principal directions
    constexpr static Vec3f kSearchDirs[] = {
        Vec3f( 1.0f,  0.0f,  0.0f),
        Vec3f(-1.0f,  0.0f,  0.0f),
        Vec3f( 0.0f,  1.0f,  0.0f),
        Vec3f( 0.0f, -1.0f,  0.0f),
        Vec3f( 0.0f,  0.0f,  1.0f),
        Vec3f( 0.0f,  0.0f, -1.0f),
    };

    // 3 principal axes
    constexpr static Vec3f kAxes[] = {
        Vec3f(1.0f, 0.0f, 0.0f),
        Vec3f(0.0f, 1.0f, 0.0f),
        Vec3f(0.0f, 0.0f, 1.0f)
    };

    SupportVert vert[4];
    int num;

    Simplex() : num(0) {}
    void BlowingUp(Collider*a, Collider* b);
};

}
}
