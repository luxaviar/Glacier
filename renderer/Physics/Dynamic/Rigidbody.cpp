#include "rigidbody.h"
#include <algorithm>
#include <assert.h>
#include "physics/world.h"
#include "physics/collider/collider.h"
#include "Core/GameObject.h"

namespace glacier {

uint32_t Rigidbody::body_counter_ = 0;

Rigidbody::Rigidbody(RigidbodyType type, bool use_gravity) :
    type_(type),
    displacement_(0.0f),
    //world_(nullptr),
    filter_(nullptr),
    tx_ver_(0),
    mass_(0),
    inv_mass_(0),
    inv_inertia_ver_(0),
    use_gravity_(use_gravity),
    force_(0.0f),
    torque_(0.0f),
    linear_velocity_(0.0f),
    angular_velocity_(0.0f),
    linear_damping_(0),
    angular_damping_(0),
    sleep_time_(0),
    asleep_(false),
    island_ver_(0)
{
}

Rigidbody::~Rigidbody() {
}

void Rigidbody::OnAwake() {
    float mass = 0.0f;
    auto fn = [this, &mass](Collider* collider) {
        if (!collider->rigidbody_) {
            collider->rigidbody_ = this;
            this->colliders_.push_back(collider);

            if (collider->IsActive() && !collider->is_sensor()) {
                mass += collider->mass();
            }
        }
    };

    game_object()->VisitComponents<Collider>(fn);
    game_object()->VisitComponentsInChildren<Collider>(fn);

    if (mass > 0.0f) {
        UpdateMass();
    }
}

void Rigidbody::OnEnable() {
    physics::World::Instance()->Add(node_);

    tx_ver_ = transform().version();
    Awake();
}

void Rigidbody::OnDisable() {
    physics::World::Instance()->Remove(node_);
}

void Rigidbody::OnDestroy() {
    OnDisable();
}

Vec3f Rigidbody::mass_center() const {
    return transform().ApplyTransform(local_mass_center_);
}

float Rigidbody::mass() const {
    return mass_;
}

float Rigidbody::inv_mass() const {
    return inv_mass_;
}

const Vec3f& Rigidbody::force() const {
    return force_;
}

const Vec3f& Rigidbody::torque() const {
    return torque_;
}

const Vec3f& Rigidbody::linear_velocity() const {
    return linear_velocity_;
}

void Rigidbody::linear_velocity(const Vec3f& vel) {
    //if (type_ == RigidbodyType::kStatic) return;
    linear_velocity_ = vel;
}

const Vec3f& Rigidbody::angular_velocity() const {
    return angular_velocity_;
}

void Rigidbody::angular_velocity(const Vec3f& vel) {
    //if (type_ == RigidbodyType::kStatic) return;
    angular_velocity_ = vel;
}

float Rigidbody::linear_damping() const {
    return linear_damping_; 
}

void Rigidbody::linear_damping(float damping) {
    linear_damping_ = damping;
}

float Rigidbody::angular_damping() const {
    return angular_damping_;
}

void Rigidbody::angular_damping(float damping) {
    angular_damping_ = damping;
}

Vec3f Rigidbody::acceleration() {
    return force_ * inv_mass_;
}

const Matrix3x3& Rigidbody::inv_inertia_tensor() {
    if (inv_inertia_ver_ != transform().version()) {
        inv_inertia_ver_ = transform().version();
        Matrix3x3 rot = transform().rotation().ToMatrix();
        inv_inertia_tensor_ =  rot * local_inv_inertia_tensor_ * rot.Transposed();
    }

    return inv_inertia_tensor_;
}

bool Rigidbody::is_kinematic() const {
    return type_ == RigidbodyType::kKinematic;
}

bool Rigidbody::is_dynamic() const {
    return type_ == RigidbodyType::kDynamic;
}

const Vec3f& Rigidbody::displacement() const {
    return displacement_;
}

bool Rigidbody::asleep() const {
    return asleep_;
}

float Rigidbody::sleep_time() const {
    return sleep_time_;
}

void Rigidbody::UpdateMass() {
    if (game_object()->IsDying()) return;

    mass_ = 0;
    local_mass_center_.Zero();
    local_inv_inertia_tensor_.Zero();

    if (!is_dynamic()) {
        inv_mass_ = 0;
        return;
    }

    auto& tx = transform();
    for (auto collider : colliders_) {
        if (!collider->IsActive() || collider->is_sensor()) continue;

        mass_ += collider->mass();
        const Transform& shapeTransform = collider->transform();
        if (&shapeTransform != &tx) {
            local_mass_center_ += shapeTransform.local_position() * collider->mass();
        }
    }

    if (mass_ > 0.0f) {
        inv_mass_ = 1.0f / mass_;
        local_mass_center_ = local_mass_center_ * inv_mass_;
    }

    for (auto collider : colliders_) {
        if (!collider->IsActive() || collider->is_sensor()) continue;

        const Transform& shapeTransform = collider->transform();

        Matrix3x3 inertiaTensor = collider->CalcInertiaTensor(collider->mass());
        Vec3f offset = Vec3f::zero;
        if (&shapeTransform != &tx) {
            Matrix3x3 rot = shapeTransform.local_rotation().ToMatrix();
            inertiaTensor = rot * inertiaTensor * rot.Transposed();
            offset = shapeTransform.local_position() - local_mass_center_;
        }

        float offsetSquare = offset.MagnitudeSq();
        if (offsetSquare > math::kEpsilonSq) {
            Matrix3x3 offsetMatrix(
                offsetSquare, 0.0f, 0.0f,
                0.0f, offsetSquare, 0.0f,
                0.0f, 0.0f, offsetSquare
            );

            offsetMatrix[0] += offset * (-offset.x);
            offsetMatrix[1] += offset * (-offset.y);
            offsetMatrix[2] += offset * (-offset.z);
            offsetMatrix *= collider->mass();

            inertiaTensor += offsetMatrix;
        }

        local_inv_inertia_tensor_ += inertiaTensor;
    }

    if (!local_inv_inertia_tensor_.Inverse()) {
        //LOG_WARN << "local inertia tensor inverse failed! for rigidbody " << id_;
    }
    //inv_inertia_tensor();

    inv_inertia_ver_ = transform().version();
    Matrix3x3 rot = transform().rotation().ToMatrix();
    inv_inertia_tensor_ = rot * local_inv_inertia_tensor_ * rot.Transposed();
}

void Rigidbody::ApplyForce(const Vec3f& force) {
    if (!is_dynamic()) return;
    force_ += force;
    Awake();
}

void Rigidbody::ApplyTorque(const Vec3f& torque) {
    if (!is_dynamic()) return;
    torque_ += torque;
    Awake();
}

void Rigidbody::ClearForce() {
    force_.Zero();
    torque_.Zero();
}

void Rigidbody::FixedUpdate(float deltaTime) {
    if (!IsActive()) return;

    IntegrateForce(deltaTime);
}

void Rigidbody::IntegratePosition(float deltaTime) {
    if (!IsActive()) return;

    if (is_kinematic()) {
        return;
    }

    if (!is_dynamic() || asleep_) return;

    Vec3f massCenter = Vec3f::zero;

    if (linear_velocity_ != Vec3f::zero) {
         massCenter = mass_center() + linear_velocity_ * deltaTime;
    }
    
    auto& tx = transform();
    if (angular_velocity_ != Vec3f::zero) {
        Quaternion rot = tx.rotation();
        Quaternion tmp(angular_velocity_.x, angular_velocity_.y, angular_velocity_.z, 0);
        rot = (rot + tmp * rot * deltaTime * 0.5f);
        rot.Normalize();

        Vec3f old = transform().position();
        Vec3f pos = massCenter - rot * local_mass_center_;
        tx.SetPositionAndRotation(pos, rot);

        displacement_ = pos - old;
    } else if (linear_velocity_ != Vec3f::zero) {
        Vec3f old = transform().position();
        Vec3f pos = massCenter - tx.rotation() * local_mass_center_;
        tx.position(pos);

        displacement_ = pos - old;
    }
}

void Rigidbody::IntegrateForce(float deltaTime) {
    if (!is_dynamic()) return;
    if (asleep_) {
        ClearForce();
        return;
    }

    if (use_gravity_) {
        linear_velocity_ += (acceleration() + physics::World::Instance()->gravity()) * deltaTime;
    }
    else {
        linear_velocity_ += acceleration() * deltaTime;
    }
    angular_velocity_ += inv_inertia_tensor() * torque_ * deltaTime;

    // Apply local damping.
    // ODE: dv/dt + c * v = 0
    // Solution: v(t) = v0 * exp(-c * t)
    // Step: v(t + dt) = v0 * exp(-c * (t + dt)) = v0 * exp(-c * t) * exp(-c * dt) = v * exp(-c * dt)
    // v2 = exp(-c * dt) * v1
    // Padé approximation:
    // 1 / (1 + c * dt)
    if (linear_damping_ != 0.0f) {
        float damping = 1.0f / (1.0f + linear_damping_ * deltaTime);
        linear_velocity_ *= damping;
    }

    if (angular_damping_ != 0.0f) {
        float damping = 1.0f / (1.0f + angular_damping_ * deltaTime);
        angular_velocity_ *= damping;
    }

    ClearForce();
}

void Rigidbody::AddIsland(uint32_t ver) {
    island_ver_ = ver;
}

bool Rigidbody::IsOnIsland(uint32_t ver) {
    return island_ver_ == ver;
}

void Rigidbody::Awake(bool flagonly) {
    if (!flagonly) {
        sleep_time_ = 0.0f;
    }
    asleep_ = false;
}

void Rigidbody::Sleep(float dt) {
    sleep_time_ += dt;
}

void Rigidbody::SetAsleep() {
    asleep_ = true;
    linear_velocity_.Zero();
    angular_velocity_.Zero();
}

}

