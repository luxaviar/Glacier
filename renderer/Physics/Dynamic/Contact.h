#pragma once

#include "Math/Util.h"
#include "Math/Vec3.h"
#include "Math/Mat3.h"

namespace glacier {

class Rigidbody;
class Collider;

namespace physics {

struct ContactPoint;
class ContactManifold;

struct Contact {
    int id;
    ContactManifold* manifold;

    Vec3f pointA;
    Vec3f pointB;
    Vec3f globalPointA;
    Vec3f globalPointB;

    Vec3f localPointA;
    Vec3f localPointB;

    Vec3f normal; //from A to B
    Vec3f tangent[2];

    float depth;
    float normalImpulseSum;
    float tangentImpulseSum[2];
    bool persistent;

    Contact(ContactManifold* manifold, const ContactPoint& ci);
    void WarmStart(float ratio);
    void Solve(float invDeltaTime);
    void SolveTangent(Vec3f& velocityA, Vec3f& omegaA, Vec3f& velocityB, Vec3f& omegaB, 
           const Vec3f& ra, const Vec3f& rb, const Matrix3x3& invIA, const Matrix3x3& invIB, 
            float invDeltaTime, float lo, float hi, int idx);
    void SolveNormal(Vec3f& velocityA, Vec3f& omegaA, Vec3f& velocityB, Vec3f& omegaB, 
           const Vec3f& ra, const Vec3f& rb, const Matrix3x3& invIA, const Matrix3x3& invIB, float invDeltaTime);

    void Clear();

    void OnDrawGizmos() const;
};

}
}
