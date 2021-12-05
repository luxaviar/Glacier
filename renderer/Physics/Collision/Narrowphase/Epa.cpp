#include "Epa.h"
#include <math.h>
#include <limits>
#include "Geometry/Triangle.h"
#include "Physics/Collision/ContactPoint.h"
//#include "unico/log.h"

namespace glacier {
namespace physics {

#ifdef WIN32
#undef min
#undef max
#endif

Epa::Epa(int max_iteration, float threshold) : 
    max_iteration_(max_iteration), 
    grow_threshold_(threshold), 
    tri_allocator_(kMaxTriangleSize), 
    edge_allocator_(kMaxEdgeSize) {
}

bool Epa::Solve(Collider* a, Collider* b, Simplex& simplex, ContactPoint& ci) {
    simplex.BlowingUp(a, b);

    for (auto tri : triangles_) {
        tri_allocator_.Release(tri);
    }

    SupportVert* vert = simplex.vert;
    triangles_.clear();
    triangles_.push_back(tri_allocator_.Acquire(vert[0], vert[1], vert[2]));
    triangles_.push_back(tri_allocator_.Acquire(vert[0], vert[2], vert[3]));
    triangles_.push_back(tri_allocator_.Acquire(vert[0], vert[3], vert[1]));
    triangles_.push_back(tri_allocator_.Acquire(vert[1], vert[3], vert[2]));
    
    float min_growth = std::numeric_limits<float>::max();
    float closest_dist = std::numeric_limits<float>::max();
    int iter = max_iteration_;

    SupportTriangle* closest_tri = nullptr;
    
    while (--iter > 0) {
        // find closest triangle to origin
        if (closest_tri == nullptr) {
            closest_dist = std::numeric_limits<float>::max();
            for (auto tri : triangles_) {
                if (math::Abs(tri->distance) < closest_dist) {
                    closest_dist = math::Abs(tri->distance);
                    closest_tri = tri;
                }
            }

            if (closest_tri == nullptr) {
                return false;
            }
        }

        Vec3f normal = closest_tri->normal;
        SupportVert new_support = MinkowskiSum::PackSupportPoint(a, b, normal);
        
        float new_dist = math::Abs(new_support.point.Dot(normal));
        float growth = math::Abs(new_dist - closest_dist);
        if (growth < min_growth) {
            min_growth = growth;
            if (min_growth < grow_threshold_) {
                return ExtraContactManifold(closest_tri, ci);
            }
        }

        for (auto edge : edges_) {
            edge_allocator_.Release(edge);
        }

        edges_.clear();

        int total = 0;
        for (int i = static_cast<int>(triangles_.size()) - 1; i >= 0; --i) {
            SupportTriangle* tri = triangles_[i];
            // can this face be 'seen' by entry_new_support?
            
            if (tri->normal.Dot(new_support.point - tri->a.point) > 0.0f) {
                AddEdge(tri->a, tri->b);
                AddEdge(tri->b, tri->c);
                AddEdge(tri->c, tri->a);
                triangles_.erase(triangles_.begin() + i);
                tri_allocator_.Release(tri);

                if (tri == closest_tri) {
                    closest_tri = nullptr;
                }
                total++;
            }
        }

        // create new triangles from the edges in the edge list
        for (auto edge : edges_) {
            SupportTriangle* new_tri = tri_allocator_.Acquire(new_support, edge->a, edge->b);
            triangles_.push_back(new_tri);
            if (triangles_.size() > kMaxTriangleSize) {
                return false;
            }

            if (math::Abs(new_tri->distance) < closest_dist) {
                closest_dist = math::Abs(new_tri->distance);
                closest_tri = new_tri;
            }
        }
    }

    return false;
}

void Epa::AddEdge(const SupportVert& a, const SupportVert& b) {
    for (size_t i = 0; i < edges_.size(); ++i) {
        SupportEdge* edge = edges_[i];
        if (edge->a.id == b.id && edge->b.id == a.id) {
            edges_.erase(edges_.begin() + i);
            edge_allocator_.Release(edge);
            return;
        }
    }
    
    SupportEdge* se = edge_allocator_.Acquire(a, b);
    edges_.push_back(se);
}

bool Epa::ExtraContactManifold(SupportTriangle* tri, ContactPoint& ci) {
    float distFromOrigin = tri->normal.Dot(tri->a.point);

    float baryU, baryV, baryW;
    Triangle::Barycentric(tri->normal * distFromOrigin, tri->a.point, tri->b.point, tri->c.point, baryU, baryV, baryW);

    if (::isnan(baryU) ||
        ::isnan(baryV) ||
        ::isnan(baryW)) {
        return false;
    }

    if (math::Abs(baryU) > 1.0f ||
        math::Abs(baryV) > 1.0f ||
        math::Abs(baryW) > 1.0f) {
        return false;
    }

    Vec3f pointA(tri->a.pointA * baryU + tri->b.pointA * baryV + tri->c.pointA * baryW);
    Vec3f pointB(tri->a.pointB * baryU + tri->b.pointB * baryV + tri->c.pointB * baryW);

    float penetrationDepth = distFromOrigin;
    ci.pointA = pointA;// - normal;
    ci.pointB = pointB;
    ci.normal = tri->distance < 0 ? -tri->normal : tri->normal; //from a to b
    ci.penetration = penetrationDepth;

    return true;
}

}
}

