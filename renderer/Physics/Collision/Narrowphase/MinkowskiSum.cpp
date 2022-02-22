#include "MinkowskiSum.h"
#include "Physics/Collider/Collider.h"
#include "Math/Util.h"

namespace glacier {
namespace physics {

uint32_t SupportVert::id_counter_ = 0;

SupportVert::SupportVert() {
}

SupportVert::SupportVert(const Vec3f& point_, const Vec3f& pointA_, const Vec3f& pointB_) :
    point(point_),
    pointA(pointA_),
    pointB(pointB_),
    id(++id_counter_) {
}

constexpr Vec3f Simplex::kSearchDirs[];
constexpr Vec3f Simplex::kAxes[];

void Simplex::BlowingUp(Collider* a, Collider* b) {
    // blow up simplex to tetrahedron
    switch (num) {
        case 1:
        {
            for (const auto& dir : kSearchDirs) {
                vert[1] = MinkowskiSum::PackSupportPoint(a, b, dir);
                if ((vert[1].point - vert[0].point).MagnitudeSq() >= math::kEpsilon)
                    break;
            }
        }
        case 2:
        {
            Vec3f lineVec = vert[1].point - vert[0].point;
            int leastSignificantAxis = lineVec.LeastSignificantComponent();
            Vec3f searchDir = lineVec.Cross(kAxes[leastSignificantAxis]);
            Matrix3x3 rot = Quaternion::AngleAxis(60.0f, lineVec).ToMatrix();

            for (int i = 0; i < 6; ++i) {
                vert[2] = MinkowskiSum::PackSupportPoint(a, b, searchDir.Normalized());

                if (vert[2].point.MagnitudeSq() > math::kEpsilon)
                    break;

                searchDir = rot * searchDir;
            }
        }
        case 3:
        {
            Vec3f v01 = vert[1].point - vert[0].point;
            Vec3f v02 = vert[2].point - vert[0].point;
            Vec3f searchDir = v01.Cross(v02).Normalized();

            vert[3] = MinkowskiSum::PackSupportPoint(a, b, searchDir);
            if (vert[3].point.MagnitudeSq() < math::kEpsilonSq) {
                vert[3] = MinkowskiSum::PackSupportPoint(a, b, searchDir.Neg());
            }

            //if (searchDir.Dot(vert[0].point) > 0) {
                //vert[3] = MinkowskiSum::PackSupportPoint(a, b, searchDir.Neg());
            //} else {
                //vert[3] = MinkowskiSum::PackSupportPoint(a, b, searchDir);
            //}
        }
    }

    num = 4;

    // fix tetrahedron winding
    // so that simplex[0]-simplex[1]-simplex[2] is CW winding
    Vec3f v30 = vert[0].point - vert[3].point;
    Vec3f v31 = vert[1].point - vert[3].point;
    Vec3f v32 = vert[2].point - vert[3].point;
    float det = v30.Dot(v31.Cross(v32));
    if (det < 0.0f) {
        auto tmp = vert[0];
        vert[0] = vert[1];
        vert[1] = tmp;
    }
}
 
//void MinkowskiSum::Set(Shape* a_, Shape* b_) {
//    a = a_;
//    b = b_;
//}

Vec3f MinkowskiSum::StartDir(Collider* a, Collider* b) {
    return a->position() - b->position();
}

SupportVert MinkowskiSum::PackSupportPoint(Collider* a, Collider* b, const Vec3f& wdir) {
    Vec3f pa = a->FarthestPoint(wdir);
    Vec3f pb = b->FarthestPoint(-wdir);

    return SupportVert(pa - pb, pa, pb);
}

Vec3f MinkowskiSum::StartDir(const AABB& a, Collider* b) {
    return a.Center() - b->position();
}

SupportVert MinkowskiSum::PackSupportPoint(const AABB& a, Collider* b, const Vec3f& wdir) {
    Vec3f pa = a.FarthestPoint(wdir);
    Vec3f pb = b->FarthestPoint(-wdir);

    return SupportVert(pa - pb, pa, pb);
}

}
}

