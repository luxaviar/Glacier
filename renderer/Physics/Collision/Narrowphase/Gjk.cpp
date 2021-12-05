#include "gjk.h"
//#include "unico/lucid.h"

namespace glacier {
namespace physics {

Gjk::Gjk(int max_iteration) : max_iteration_(max_iteration) {
}

bool Gjk::Intersect(const AABB& a, Collider* b, Simplex& simplex) {
    Vec3f dir = MinkowskiSum::StartDir(a, b);
    
    simplex.vert[0] = MinkowskiSum::PackSupportPoint(a, b, dir.Normalized());
    if (simplex.vert[0].point.Dot(dir) < 0.0f) {
        return false;
    }

    simplex.num = 1;
    dir = -simplex.vert[0].point;
    int iter = max_iteration_;

    while (--iter > 0) {
        //dir.Normalized();
        SupportVert newSupport = MinkowskiSum::PackSupportPoint(a, b, dir.Normalized());
        if (newSupport.point.Dot(dir) < 0.0f) {
            return false;
        }

        simplex.vert[simplex.num++] = newSupport;
        if (UpdateSimplex(simplex, dir)) {
            return true;
        }
    }

    return false;
}

bool Gjk::Intersect(Collider* a, Collider* b, Simplex& simplex) {
    Vec3f dir = MinkowskiSum::StartDir(a, b);
    //dir.Normalized();

    simplex.vert[0] = MinkowskiSum::PackSupportPoint(a, b, dir.Normalized());
    if (simplex.vert[0].point.Dot(dir) < 0.0f) {
        return false;
    }

    simplex.num = 1;
    dir = -simplex.vert[0].point;
    int iter = max_iteration_;

    while (--iter > 0) {
        //dir.Normalized();
        SupportVert newSupport = MinkowskiSum::PackSupportPoint(a, b, dir.Normalized());
        if (newSupport.point.Dot(dir) < 0.0f) {
            return false;
        }

        simplex.vert[simplex.num++] = newSupport;
        if (UpdateSimplex(simplex, dir)) {
            return true;
        }
    }

    return false;
}

bool Gjk::UpdateSimplex(Simplex& simplex, Vec3f& dir) {
    SupportVert (&vert)[4] = simplex.vert;
    int& n = simplex.num;

    if (n == 2) {
        // Four voronoi regions that the origin could be in:
        // 0) closest to vertex s[0].
        // 1) closest to vertex s[1].
        // 2) closest to line segment s[0]->s[1]. XX
        // 3) contained in the line segment s[0]->s[1], and our search is over and the algorithm is now finished. XX

        // By construction of the simplex, the cases 0) and 1) can never occur. Then only the cases marked with XX need to be checked.

        Vec3f d01 = vert[1].point - vert[0].point;
        Vec3f newSearchDir = d01.Cross(d01.Cross(vert[1].point));
        if (newSearchDir.MagnitudeSq() > math::kEpsilon) {
            dir = newSearchDir; // Case 2)
            return false;
        } else {
            // Case 3)
            return true;
        }
    }
    else if (n == 3) {
        // Nine voronoi regions:
        // 0) closest to vertex s[0].
        // 1) closest to vertex s[1].
        // 2) closest to vertex s[2].
        // 3) closest to edge s[0]->s[1].
        // 4) closest to edge s[1]->s[2].  XX
        // 5) closest to edge s[0]->s[2].  XX
        // 6) closest to the triangle s[0]->s[1]->s[2], in the positive side.  XX
        // 7) closest to the triangle s[0]->s[1]->s[2], in the negative side.  XX
        // 8) contained in the triangle s[0]->s[1]->s[2], and our search is over and the algorithm is now finished.  XX

        // By construction of the simplex, the origin must always be in a voronoi region that includes the point s[2], since that
        // was the last added point. But it cannot be the case 2), since previous search took us deepest towards the direction s[1]->s[2],
        // and case 2) implies we should have been able to go even deeper in that direction, or that the origin is not included in the convex shape,
        // a case which has been checked for already before. Therefore the cases 0)-3) can never occur. Only the cases marked with XX need to be checked.
        Vec3f d12 = vert[2].point - vert[1].point;
        Vec3f d02 = vert[2].point - vert[0].point;
        Vec3f triNormal = d02.Cross(d12);

        Vec3f e12 = d12.Cross(triNormal);
        float t12 = vert[1].point.Dot(e12);
        if (t12 < 0.0f) {
            dir = d12.Cross(d12.Cross(vert[1].point));
            vert[0] = vert[1];
            vert[1] = vert[2];
            n = 2;
            return false;
        }
        Vec3f e02 = triNormal.Cross(d02);
        float t02 = vert[0].point.Dot(e02);
        if (t02 < 0.0f) {
            dir = d02.Cross(d02.Cross(vert[0].point));
            vert[1] = vert[2];
            n = 2;
            return false;
        }
        float scaledSignedDistToTriangle = triNormal.Dot(vert[2].point);
        float distSq = scaledSignedDistToTriangle * scaledSignedDistToTriangle;
        float scaledEpsilonSq = 1e-6f * triNormal.MagnitudeSq();

        if (distSq > scaledEpsilonSq) {
            // The origin is sufficiently far away from the triangle.
            if (scaledSignedDistToTriangle <= 0.0f) {
                dir = triNormal;
                return false; // Case 6)
            } else {
                // Case 7) Swap s[0] and s[1] so that the normal of Triangle(s[0],s[1],s[2]).PlaneCCW() will always point towards the new search direction.
                SupportVert tmp = vert[0];
                vert[0] = vert[1];
                vert[1] = tmp;
                dir = -triNormal;
                return false;
            }
        }
        else {
            // Case 8) The origin lies directly inside the triangle. For robustness, terminate the search here immediately with success.
            return true;
        }
    }
    else {//(n == 4)
          // A tetrahedron defines fifteen voronoi regions:
          //  0) closest to vertex s[0].
          //  1) closest to vertex s[1].
          //  2) closest to vertex s[2].
          //  3) closest to vertex s[3].
          //  4) closest to edge s[0]->s[1].
          //  5) closest to edge s[0]->s[2].
          //  6) closest to edge s[0]->s[3].  XX
          //  7) closest to edge s[1]->s[2].
          //  8) closest to edge s[1]->s[3].  XX
          //  9) closest to edge s[2]->s[3].  XX
          // 10) closest to the triangle s[0]->s[1]->s[2], in the outfacing side.
          // 11) closest to the triangle s[0]->s[1]->s[3], in the outfacing side. XX
          // 12) closest to the triangle s[0]->s[2]->s[3], in the outfacing side. XX
          // 13) closest to the triangle s[1]->s[2]->s[3], in the outfacing side. XX
          // 14) contained inside the tetrahedron simplex, and our search is over and the algorithm is now finished. XX

        // By construction of the simplex, the origin must always be in a voronoi region that includes the point s[3], since that
        // was the last added point. But it cannot be the case 3), since previous search took us deepest towards the direction s[2]->s[3],
        // and case 3) implies we should have been able to go even deeper in that direction, or that the origin is not included in the convex shape,
        // a case which has been checked for already before. Therefore the cases 0)-5), 7) and 10) can never occur and
        // we only need to check cases 6), 8), 9), 11), 12), 13) and 14), marked with XX.
        Vec3f d01 = vert[1].point - vert[0].point;
        Vec3f d02 = vert[2].point - vert[0].point;
        Vec3f d03 = vert[3].point - vert[0].point;
        Vec3f tri013Normal = d01.Cross(d03); // Normal of triangle 0->1->3 pointing outwards from the simplex.
        Vec3f tri023Normal = d03.Cross(d02); // Normal of triangle 0->2->3 pointing outwards from the simplex.
        Vec3f e03_1 = tri013Normal.Cross(d03); // The normal of edge 0->3 on triangle 013.
        Vec3f e03_2 = d03.Cross(tri023Normal); // The normal of edge 0->3 on triangle 023.
        float inE03_1 = e03_1.Dot(vert[3].point);
        float inE03_2 = e03_2.Dot(vert[3].point);
        if (inE03_1 <= 0.0f && inE03_2 <= 0.0f) {
            // Case 6) Edge 0->3 is closest. Simplex degenerates to a line segment.
            dir = d03.Cross(d03.Cross(vert[3].point));
            vert[1] = vert[3];
            n = 2;
            return false;
        }

        Vec3f d12 = vert[2].point - vert[1].point;
        Vec3f d13 = vert[3].point - vert[1].point;
        Vec3f tri123Normal = d12.Cross(d13);

        Vec3f e13_0 = d13.Cross(tri013Normal);
        Vec3f e13_2 = tri123Normal.Cross(d13);
        float inE13_0 = e13_0.Dot(vert[3].point);
        float inE13_2 = e13_2.Dot(vert[3].point);
        if (inE13_0 <= 0.0f && inE13_2 <= 0.0f) {
            // Case 8) Edge 1->3 is closest. Simplex degenerates to a line segment.
            dir = d13.Cross(d13.Cross(vert[3].point));
            vert[0] = vert[1];
            vert[1] = vert[3];
            n = 2;
            return false;
        }

        Vec3f d23 = vert[3].point - vert[2].point;
        Vec3f e23_0 = tri023Normal.Cross(d23);
        Vec3f e23_1 = d23.Cross(tri123Normal);
        float inE23_0 = e23_0.Dot(vert[3].point);
        float inE23_1 = e23_1.Dot(vert[3].point);
        if (inE23_0 <= 0.0f && inE23_1 <= 0.0f) {
            // Case 9) Edge 2->3 is closest. Simplex degenerates to a line segment.
            dir = d23.Cross(d23.Cross(vert[3].point));
            vert[0] = vert[2];
            vert[1] = vert[3];
            n = 2;
            return false;
        }

        float inTri013 = vert[3].point.Dot(tri013Normal);
        if (inTri013 < 0.0f && inE13_0 >= 0.0f && inE03_1 >= 0.0f) {
            // Case 11) Triangle 0->1->3 is closest.
            vert[2] = vert[3];
            n = 3;
            dir = tri013Normal;
            return false;
        }
        float inTri023 = vert[3].point.Dot(tri023Normal);
        if (inTri023 < 0.0f && inE23_0 >= 0.0f && inE03_2 >= 0.0f) {
            // Case 12) Triangle 0->2->3 is closest.
            vert[1] = vert[0];
            vert[0] = vert[2];
            vert[2] = vert[3];
            n = 3;
            dir = tri023Normal;
            return false;
        }
        float inTri123 = vert[3].point.Dot(tri123Normal);
        if (inTri123 < 0.0f && inE13_2 >= 0.0f && inE23_1 >= 0.0f) {
            // Case 13) Triangle 1->2->3 is closest.
            vert[0] = vert[1];
            vert[1] = vert[2];
            vert[2] = vert[3];
            n = 3;
            dir = tri123Normal;
            return false;
        }

        // Case 14) Not in the voronoi region of any triangle or edge. The origin is contained in the simplex, the search is finished.
        return true;
    }
}

}
}
