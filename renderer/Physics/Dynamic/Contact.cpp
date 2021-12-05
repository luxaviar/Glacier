#include "Contact.h"
#include "Physics/Collision/ContactPoint.h"
#include "ContactManifold.h"
#include "Physics/Collider/Collider.h"
#include "Physics/Dynamic/Rigidbody.h"
#include "Physics/World.h"
#include "imgui/imgui.h"
#include "Render/Editor/Gizmos.h"

namespace glacier {
namespace physics {

Contact::Contact(ContactManifold* cm, const ContactPoint& ci) :
    manifold(cm),
    pointA(ci.pointA),
    pointB(ci.pointB),
    globalPointA(ci.pointA),
    globalPointB(ci.pointB),
    normal(ci.normal),
    depth(ci.penetration),
    normalImpulseSum(0.0f),
    tangentImpulseSum{0.0f, 0.0f},//(0.0f),
    //tangentImpulseSum2(0.0f),
    persistent(false)
{
    //point = (pointA + pointB) / 2f;
    localPointA = cm->a_->transform().InverseTransform(pointA);
    localPointB = cm->b_->transform().InverseTransform(pointB);

    //http://box2d.org/2014/02/computing-a-basis/
    if (math::Abs(normal.x) >= 0.57735f) {
        tangent[0].Set(normal.y, -normal.x, 0.0f);
    } else {
        tangent[0].Set(0.0f, normal.z, -normal.y);
    }

    tangent[0].Normalized();
    tangent[1] = normal.Cross(tangent[0]);

    // Friction mixing. The idea is to allow a very low friction value to
    // drive down the mixing result. Example: anything slides on ice.
    //friction = math::Sqrt(fta->friction() * ftb->friction());

    // Restitution mixing. The idea is to use the maximum bounciness, so bouncy
    // objects will never not bounce during collisions.
    //restitution = math::Max(fta->restitution(), ftb->restitution());
}

void Contact::WarmStart(float ratio) {
    if (!persistent) return;

    Vec3f normalImpulse = normal * normalImpulseSum;
    Vec3f tangentImpulse1 = tangent[0] * tangentImpulseSum[0];
    Vec3f tangentImpulse2 = tangent[1] * tangentImpulseSum[1];
    Vec3f P = (normalImpulse + tangentImpulse1 + tangentImpulse2) * ratio;

    //FIXME: do we need update global point?
    pointA = manifold->a_->transform().ApplyTransform(localPointA);
    pointB = manifold->b_->transform().ApplyTransform(localPointB);

    Rigidbody* bodyA = manifold->a_->rigidbody();
    if (bodyA && bodyA->is_dynamic()) {
        Vec3f ra = pointA - bodyA->mass_center();
        bodyA->linear_velocity(bodyA->linear_velocity() - P * bodyA->inv_mass());
        bodyA->angular_velocity(bodyA->angular_velocity() - bodyA->inv_inertia_tensor() * (ra.Cross(P)));
    }

    Rigidbody* bodyB = manifold->b_->rigidbody();
    if (bodyB && bodyB->is_dynamic()) {
        Vec3f rb = pointB - bodyB->mass_center();
        bodyB->linear_velocity(bodyB->linear_velocity() + P * bodyB->inv_mass());
        bodyB->angular_velocity(bodyB->angular_velocity() + bodyB->inv_inertia_tensor() * (rb.Cross(P)));
    }
}

void Contact::Solve(float invDeltaTime) {
    Rigidbody* bodyA = manifold->a_->rigidbody();
    Rigidbody* bodyB = manifold->b_->rigidbody();

    Vec3f velocityA = bodyA ? bodyA->linear_velocity() : Vec3f::zero;
    Vec3f velocityB = bodyB ? bodyB->linear_velocity() : Vec3f::zero;

    Vec3f omegaA = bodyA ? bodyA->angular_velocity() : Vec3f::zero;
    Vec3f omegaB = bodyB ? bodyB->angular_velocity() : Vec3f::zero;

    const Matrix3x3& invIA = bodyA ? bodyA->inv_inertia_tensor() : Matrix3x3::zero;
    const Matrix3x3& invIB = bodyB ? bodyB->inv_inertia_tensor() : Matrix3x3::zero;

    Vec3f ra = pointA - (bodyA ? bodyA->mass_center() : manifold->a_->position());
    Vec3f rb = pointB - (bodyB ? bodyB->mass_center() : manifold->b_->position());

    SolveNormal(velocityA, omegaA, velocityB, omegaB, ra, rb, invIA, invIB, invDeltaTime);

    float hi = manifold->friction_ * normalImpulseSum;
    float lo = -hi;

    SolveTangent(velocityA, omegaA, velocityB, omegaB, ra, rb, invIA, invIB, invDeltaTime, lo, hi, 0);
    SolveTangent(velocityA, omegaA, velocityB, omegaB, ra, rb, invIA, invIB, invDeltaTime, lo, hi, 1);

    if (bodyA && bodyA->is_dynamic()) {
        bodyA->linear_velocity(velocityA);
        bodyA->angular_velocity(omegaA);
    }

    if (bodyB && bodyB->is_dynamic()) {
        bodyB->linear_velocity(velocityB);
        bodyB->angular_velocity(omegaB);
    }
}

void Contact::SolveTangent(Vec3f& velocityA, Vec3f& omegaA, Vec3f& velocityB, Vec3f& omegaB, 
       const Vec3f& ra, const Vec3f& rb, const Matrix3x3& invIA, const Matrix3x3& invIB, 
       float invDeltaTime, float lo, float hi, int idx) {

    Rigidbody* bodyA = manifold->a_->rigidbody();
    Rigidbody* bodyB = manifold->b_->rigidbody();
    float inv_mass_a = bodyA ? bodyA->inv_mass() : 0.0f;
    float inv_mass_b = bodyB ? bodyB->inv_mass() : 0.0f;

    Vec3f cur_tangent = tangent[idx];
    Vec3f reltiveVelocity = (velocityB + omegaB.Cross(rb) - velocityA - omegaA.Cross(ra));

    Vec3f rcta = ra.Cross(cur_tangent); //ra X n
    Vec3f rctb = rb.Cross(cur_tangent); //rb X n

    Vec3f idrcta = invIA * rcta; //Ia^-1 * (ra X n)
    Vec3f idrctb = invIB * rctb; //Ib^-1 * (rb X n)

    //effective mass = J * M^-1 * J^T
    float effectiveMass = inv_mass_a + inv_mass_b + idrcta.Dot(rcta) + idrctb.Dot(rctb);
    float invEffectiveMass = 0.0f;
    if (effectiveMass > 0.0f) {
        invEffectiveMass = 1.0f / effectiveMass;
    }

    //JV = relativeVelocity project to normal
    float relvt = (reltiveVelocity).Dot(cur_tangent);
    float lambda = -relvt * invEffectiveMass;

    float impulse_sum = tangentImpulseSum[idx];
    tangentImpulseSum[idx] = math::Clamp(impulse_sum + lambda, lo, hi);
    lambda = tangentImpulseSum[idx] - impulse_sum;
    
    Vec3f impulse = cur_tangent * lambda;
    velocityA -= (impulse * inv_mass_a);
    omegaA -= (idrcta * lambda);

    velocityB += (impulse * inv_mass_b);
    omegaB += (idrctb * lambda);
}

void Contact::SolveNormal(Vec3f& velocityA, Vec3f& omegaA, Vec3f& velocityB, Vec3f& omegaB, 
       const Vec3f& ra, const Vec3f& rb, const Matrix3x3& invIA, const Matrix3x3& invIB, float invDeltaTime) {

    Rigidbody* bodyA = manifold->a_->rigidbody();
    Rigidbody* bodyB = manifold->b_->rigidbody();
    float inv_mass_a = bodyA ? bodyA->inv_mass() : 0.0f;
    float inv_mass_b = bodyB ? bodyB->inv_mass() : 0.0f;

    Vec3f reltiveVelocity = (velocityB + omegaB.Cross(rb) - velocityA - omegaA.Cross(ra));

    Vec3f rcna = ra.Cross(normal); //ra X n
    Vec3f rcnb = rb.Cross(normal); //rb X n

    Vec3f idrcna = invIA * rcna; //Ia^-1 * (ra X n)
    Vec3f idrcnb = invIB * rcnb; //Ib^-1 * (rb X n)

    //effective mass = J * M^-1 * J^T
    float effectiveMass = inv_mass_a + inv_mass_b + idrcna.Dot(rcna) + idrcnb.Dot(rcnb);
    float invEffectiveMass = 0.0f;
    if (effectiveMass > 0.0f) {
        invEffectiveMass = 1.0f / effectiveMass;
    }

    //JV = relativeVelocity project to normal
    float relvn = reltiveVelocity.Dot(normal);

    World* world = World::Instance();
    float d = (pointB - pointA).Dot(normal);
    float b = -(world->baumgarte_factor() * invDeltaTime) * math::Max(-d - world->penetration_slop(), 0.0f) -
        manifold->restitution_ * math::Max(-relvn - world->restitution_slop(), 0.0f);

    //lambda = -(JV + b) / effective mass
    float lambda = -(relvn + b) * invEffectiveMass;

    float oldNormalImpulseSum = normalImpulseSum;
    normalImpulseSum = math::Max(0.0f, normalImpulseSum + lambda);
    lambda = normalImpulseSum - oldNormalImpulseSum;

    Vec3f impulse = normal * lambda;
    velocityA -= (impulse * inv_mass_a);
    omegaA -= (idrcna * lambda);

    velocityB += (impulse * inv_mass_b);
    omegaB += (idrcnb * lambda);
}

void Contact::Clear() {
    //bodyA = nullptr;
    //bodyB = nullptr;
}

void Contact::OnDrawGizmos() const {
    auto center = (pointA + pointB) / 2.0f;
    //Color old = Gizmos.color;
    auto gizmos = render::Gizmos::Instance();
    //Gizmos.color = Color.black;
    gizmos->SetColor(Color::kBlack);
    gizmos->DrawSphere(center, 0.2f);
    gizmos->SetColor(Color::kRed);
    gizmos->DrawLine(center, center + normal * 1.5f);
    gizmos->SetColor(Color::kGreen);
    gizmos->DrawLine(center, center + tangent[0] * 1.5f);
    gizmos->SetColor(Color::kBlue);
    gizmos->DrawLine(center, center + tangent[1] * 1.5f);
}

}
}

