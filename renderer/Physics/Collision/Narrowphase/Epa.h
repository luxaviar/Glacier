#pragma once

#include "Common/Freelist.h"
#include "Physics/Collision/Narrowphase/MinkowskiSum.h"
#include "ContactManifoldSolver.h"

namespace glacier {

class Collider;

namespace physics {

struct ContactPoint;

struct SupportTriangle {
    SupportVert a;
    SupportVert b;
    SupportVert c;
    Vec3f normal;
    float distance; //distance to origin

    //SupportTriangle() {}
    SupportTriangle(const SupportVert& a_, const SupportVert& b_, const SupportVert& c_) {
        Reset(a_, b_, c_);
    }

    void Reset(const SupportVert& a_, const SupportVert& b_, const SupportVert& c_) {
        a = a_;
        b = b_;
        c = c_;
        normal = (b.point - a.point).Cross(c.point - a.point).Normalized();
        distance = a.point.Dot(normal);
    }
};

struct SupportEdge {
    SupportVert a;
    SupportVert b;

    //SupportEdge() {}
    SupportEdge(SupportVert a_, SupportVert b_) {
        Reset(a_, b_);
    }

    void Reset(SupportVert a_, SupportVert b_) {
        a = a_;
        b = b_;
    }
};

class Epa : public ContactManifoldSolver {
public:
    constexpr static size_t kMaxEdgeSize = 2048;
    constexpr static size_t kMaxTriangleSize = 1024;

    Epa(int max_iteration, float threshold);
    bool Solve(Collider* a, Collider* b, Simplex& simplex, ContactPoint& ci);

private:
    bool ExtraContactManifold(SupportTriangle* tri, ContactPoint& ci);
    void AddEdge(const SupportVert& a, const SupportVert& b);

private:
    int max_iteration_;// = 128;
    float grow_threshold_;// = 0.001f;

    FreeList<SupportTriangle> tri_allocator_;
    FreeList<SupportEdge> edge_allocator_;

    std::vector<SupportTriangle*> triangles_;
    std::vector<SupportEdge*> edges_;
};

}
}
