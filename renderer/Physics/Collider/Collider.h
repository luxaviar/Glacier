#pragma once

#include "Math/Util.h"
#include "Common/Uncopyable.h"
#include "Common/TypeTraits.h"
#include "Core/Transform.h"
#include "Geometry/Aabb.h"
#include "Geometry/Plane.h"
#include "Core/Component.h"
#include "Core/Identifiable.h"
#include "Common/List.h"
#include "Physics/Collision/ContactPoint.h"
#include "Physics/Dynamic/Rigidbody.h"
#include "Algorithm/Bvh.h"

namespace glacier {

class Rigidbody;

namespace physics {
struct BvhNode;
class DynamicBvh;
class ContactManifold;
class ContactSolver;
class Island;
}

enum class ShapeCategory : int8_t {
    kOBB = 1,
    kSphere = 2,
    kCapusule = 3,
    kCylinder = 4,
    kPolyHedron = 5,
};

class Collider : public Component, public Identifiable<Collider> {
public:
    friend class Rigidbody;
    friend struct physics::BvhNode;
    friend class physics::DynamicBvh;
    friend physics::ContactManifold;
    friend physics::ContactSolver;
    friend physics::Island;

    using CollisionCallback = std::function<void(const physics::ContactInfo& cp)>;
    using SensorCallback = std::function<void(Collider* self, Collider* other)>;

    //A sensor collider
    Collider(ShapeCategory category);
    Collider(ShapeCategory category, float mass, float friction, float restitution);

    virtual ~Collider();

    void OnAwake() override;
    void OnParentChange() override;

    void OnEnable() override;
    void OnDisable() override;

    ShapeCategory category() const { return category_; }
    const AABB& bounds();

    Vec3f position() const { return transform().position(); }
    Quaternion rotation() const { return transform().rotation(); }

    virtual bool Contains(const Vec3f& point) = 0;
    virtual bool Intersects(const Ray& ray, float max, float& t) = 0;

    virtual float Distance(const Vec3f& point) = 0;
    virtual Vec3f ClosestPoint(const Vec3f& point) = 0;

    //wdir should be normalized dir in world space
    virtual Vec3f FarthestPoint(const Vec3f& wdir) = 0;
    virtual Matrix3x3 CalcInertiaTensor(float mass) = 0;

    bool is_sensor() const { return is_sensor_; }
    bool is_static() const { return !rigidbody_; }

    bool is_dynamic() const { return !is_sensor_ && rigidbody_ && rigidbody_->is_dynamic(); }
    bool is_kinematic() const { return rigidbody_ && rigidbody_->is_kinematic(); }

    //not sensor, can be static or dynamic
    bool is_contactable() const { return !is_sensor_ && (!rigidbody_ || rigidbody_->is_dynamic()); }

    void SetSensor(bool v);

    float mass() const { return mass_; }
    float restitution() const { return restitution_; }
    float friction() const { return friction_; }

    void mass(float v);
    void restitution(float v) { restitution_ = v; }
    void friction(float v) { friction_ = v; }

    Rigidbody* rigidbody() const { return rigidbody_; }

    void SetCollisionEnter(CollisionCallback cb);
    void SetCollisionExit(CollisionCallback cb);
    void SetSensorEnter(SensorCallback cb);
    void SetSensorExit(SensorCallback cb);

    void OnCollisionEnter(const physics::ContactInfo& cp);
    void OnCollisionExit(const physics::ContactInfo& cp);
    void OnSensorEnter(Collider* other);
    void OnSensorExit(Collider* other);

protected:
    virtual AABB CalcAABB() = 0;

    void AttachRigidbody(Rigidbody* rigidbody);
    void DetachRigidbody();

    void AddContact(uint64_t key);
    void RemoveContact(uint64_t key);

protected:
    ShapeCategory category_;
    uint32_t bounds_version_;
    AABB bounds_;

    //dynamic
    Rigidbody* rigidbody_ = nullptr;
    BvhNode<Collider*>* node_ = nullptr;

    //material
    float mass_ = 0.0f;
    float restitution_ = 0.0f;
    float friction_ = 0.5f;

    bool is_sensor_ = false;

    CollisionCallback collision_enter_;
    CollisionCallback collision_exit_;
    SensorCallback sensor_enter_;
    SensorCallback sensor_exit_;

    std::vector<uint64_t> contacts_;
};

}
