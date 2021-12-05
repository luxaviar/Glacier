#include "collider.h"
#include <assert.h>
#include "Core/GameObject.h"
#include "physics/dynamic/rigidbody.h"
#include "physics/world.h"

namespace glacier {

Collider::Collider(ShapeCategory category) : 
    category_(category),
    bounds_version_(0),
    is_sensor_(false)
{
}

Collider::Collider(ShapeCategory category, float mass, float friction, float restitution) :
    category_(category),
    bounds_version_(0),
    mass_(mass),
    restitution_(restitution),
    friction_(friction),
    is_sensor_(false)
{

}

Collider::~Collider() {
}

void Collider::AttachRigidbody(Rigidbody* rigidbody) {
    assert(rigidbody);

    rigidbody_ = rigidbody;

    auto& list = rigidbody->colliders_;
    assert(std::find(list.begin(), list.end(), this) == list.end());

    list.push_back(this);
}

void Collider::DetachRigidbody() {
    if (!rigidbody_) return;

    auto& list = rigidbody_->colliders_;
    auto it = std::find(list.begin(), list.end(), this);
    if (it != list.end()) {
        list.erase(it);
    }

    rigidbody_ = nullptr;
    contacts_.clear();
}

void Collider::OnAwake() {
    auto rb = game_object()->GetComponent<Rigidbody>();
    if (!rb) {
        rb = game_object()->GetComponentInParent<Rigidbody>();
    }

    if (rb) {
        AttachRigidbody(rb);
    }
}

void Collider::OnParentChange() {
    auto rb = game_object()->GetComponent<Rigidbody>();
    if (!rb) {
        rb = game_object()->GetComponentInParent<Rigidbody>();
    }

    auto old_rb = rigidbody_;
    if (old_rb == rb) return;

    if (old_rb) {
        DetachRigidbody();
        if (mass_ > 0.0f && IsActive()) {
            old_rb->UpdateMass();
        }
    }

    if (rb) {
        AttachRigidbody(rb);
        if (mass_ > 0.0f && IsActive()) {
            rb->UpdateMass();
        }
    }
}

void Collider::OnEnable() {
    if (rigidbody_ && mass_ > 0.0f) {
        rigidbody_->UpdateMass();
    }

    assert(node_ == nullptr);
    physics::World::Instance()->AddCollider(this);
}

void Collider::OnDisable() {
    if (rigidbody_ && mass_ > 0.0f) {
        rigidbody_->UpdateMass();
    }

    if (node_) {
        physics::World::Instance()->RemoveCollider(this);
    }
}

void Collider::mass(float v) {
    if (mass_ == v) return;

    mass_ = v;
    if (rigidbody_ && IsActive()) {
        rigidbody_->UpdateMass();
    }
}

void Collider::SetSensor(bool v) {
    if (v == is_sensor_) return;

    is_sensor_ = v;
    if (rigidbody_ && IsActive()) {
        rigidbody_->UpdateMass();
    }
}

const AABB& Collider::bounds() {
    if (bounds_version_ != transform().version()) {
        bounds_ = CalcAABB();
        bounds_version_ = transform().version();
    }

    return bounds_;
}

void Collider::SetCollisionEnter(CollisionCallback cb) {
    collision_enter_ = cb;
}

void Collider::SetCollisionExit(CollisionCallback cb) {
    collision_exit_ = cb;
}

void Collider::SetSensorEnter(SensorCallback cb) {
    sensor_enter_ = cb;
}

void Collider::SetSensorExit(SensorCallback cb) {
    sensor_exit_ = cb;
}

void Collider::OnCollisionEnter(const physics::ContactInfo& cp) {
    if (collision_enter_)
        collision_enter_(cp);
}

void Collider::OnCollisionExit(const physics::ContactInfo& cp) {
    if (collision_exit_)
        collision_exit_(cp);
}

void Collider::OnSensorEnter(Collider* other) {
    if (sensor_enter_)
        sensor_enter_(this, other);
}

void Collider::OnSensorExit(Collider* other) {
    if (sensor_exit_)
        sensor_exit_(this, other);
}

void Collider::AddContact(uint64_t key) {
    contacts_.push_back(key);
}

void Collider::RemoveContact(uint64_t key) {
    auto it = std::find(contacts_.begin(), contacts_.end(), key);
    if (it != contacts_.end())
        contacts_.erase(it);
}

}
