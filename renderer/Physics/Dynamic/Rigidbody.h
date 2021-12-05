#pragma once

#include <functional>
#include <stdint.h>
#include "Core/Transform.h"
#include "core/identifiable.h"
#include "core/linkable.h"
#include "physics/types.h"
#include "physics/world.h"

namespace glacier {

class Collider;

namespace physics {
    class World;
    class DynamicBvh;
    class CollisionFilter;
    class Island;
}

class Rigidbody :
    public Component,
    public Identifiable<Rigidbody>,
    public Linkable<physics::World, Rigidbody> 
{
public:
    friend Collider;
    friend physics::DynamicBvh;
    friend physics::World;
    friend physics::Island;

    Rigidbody(RigidbodyType type = RigidbodyType::kDynamic, bool use_gravity = false);
    ~Rigidbody();

    void OnAwake() override;
    void OnEnable() override;
    void OnDisable() override;
    void OnDestroy() override;

    void UpdateMass();

    Vec3f mass_center() const;
    float mass() const;
    float inv_mass() const;

    const Vec3f& force() const;
    const Vec3f& torque() const;

    const Vec3f& linear_velocity() const;
    void linear_velocity(const Vec3f& vel);

    const Vec3f& angular_velocity() const;
    void angular_velocity(const Vec3f& vel);

    float linear_damping() const;
    void linear_damping(float damping);

    float angular_damping() const;
    void angular_damping(float damping);

    Vec3f acceleration();
    const Matrix3x3& inv_inertia_tensor();

    bool is_kinematic() const;
    bool is_dynamic() const;

    physics::CollisionFilter* filter() const { return filter_; }
    void filter(physics::CollisionFilter* filter) { filter_ = filter; }

    bool asleep() const;
    float sleep_time() const;
    void Awake(bool flagonly = false);

    const Vec3f& displacement() const;

    void ApplyForce(const Vec3f& force);
    void ApplyTorque(const Vec3f& torque);
    void ClearForce();
    void FixedUpdate(float deltaTime);
    void IntegratePosition(float deltaTime);
    void IntegrateForce(float deltaTime);

private:
    void AddIsland(uint32_t ver);
    bool IsOnIsland(uint32_t ver);
    void Sleep(float dt);
    void SetAsleep();

private:
    static uint32_t body_counter_;
    RigidbodyType type_;

    Vec3f displacement_;
    std::vector<Collider*> colliders_;

    physics::CollisionFilter* filter_;
    uint32_t tx_ver_;// = 0;

    float mass_;
    float inv_mass_;
    Vec3f local_mass_center_;

    uint32_t inv_inertia_ver_;
    Matrix3x3 local_inv_inertia_tensor_;
    Matrix3x3 inv_inertia_tensor_;

    bool use_gravity_;
    Vec3f force_;
    Vec3f torque_;
    Vec3f linear_velocity_;
    Vec3f angular_velocity_;

    float linear_damping_;// = 0.01f;
    float angular_damping_;// = 0.01f;

    //std::vector<uint64_t> contacts_;
    float sleep_time_;// = 0f;
    bool asleep_;// = false;
    uint32_t island_ver_;// = 0;
};

}
