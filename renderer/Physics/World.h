#pragma once

#include <vector>
#include "Geometry/Ray.h"
#include "Physics/Collision/CollidePair.h"
#include "Physics/Collision/HitResult.h"
#include "Physics/CollisionFilter.h"
#include "Common/Singleton.h"
#include "LayerCollisionFilter.h"
#include "Core/GameObject.h"
#include "Core/ObjectManager.h"

namespace glacier {

class Rigidbody;
class Collider;
class Gizmos;

namespace physics {

class CollisionFilter;
class CollisionSystem;
class ContactSolver;
class Island;
class BoardPhaseDetector;
class NarrowPhaseDetector;
class CollisionDetector;
class ContactManifoldSolver;
class ReplicaDriver;

class World : public BaseManager<World, Rigidbody> {
public:
    friend Rigidbody;
    friend ReplicaDriver;

    World();
    ~World();

    void Clear();

    void AddCollider(Collider* collider);
    void RemoveCollider(Collider* collider);
    
    void Step();
    void Advance(float dt_us);

    void Pause();
    void Resume();

    RayHitResult RayCast(const Ray& ray, float max, 
        uint32_t layer_mask = GameObject::kAllLayers, bool query_sensor = true);

    float gravity_mag() const { return gravity_mag_; }
    const Vec3f& gravity() const { return gravity_; }
    void gravity(const Vec3f& value) {
        gravity_ = value;
        gravity_mag_ = value.Magnitude();
    }
    
    int frequency() const { return frequency_; }

    float interval() const { return inv_frequency_; }
    uint32_t frame() const { return frame_; }
    
    float baumgarte_factor() const { return baumgarte_factor_; }
    float penetration_slop() const { return penetration_slop_; }
    float restitution_slop() const { return restitution_slop_; }
    float persistent_threshold_sq() const { return persistent_threshold_sq_; }

    void OnDrawGizmos(bool draw_bvh);

private:
    void FixedUpdate(float deltaTime);
    void UpdateBody(Rigidbody* body);
    void ProcessCallBack();

    static constexpr int kDefaultFrequency = 50;

    float baumgarte_factor_ = 0.2f;
    float penetration_slop_ = 0.0005f;
    float restitution_slop_ = 0.5f;
    float persistent_threshold_sq_ = 0.001f;

    bool pause_ = false;
    float delta_time_ = 0.0f;
    int frequency_ = kDefaultFrequency;
    float inv_frequency_ = 1.0f / kDefaultFrequency;
    uint32_t inv_frequency_us_ = (uint32_t)(1.0f / kDefaultFrequency * 1000000);

    float gravity_mag_ = 0;
    Vec3f gravity_ = Vec3f::zero;

    uint32_t frame_ = 0;
    
    LayerCollisionFilter filter_;
    std::unique_ptr<CollisionSystem> collision_system_;
    std::unique_ptr<ContactSolver> contact_solver_;
    std::unique_ptr<Island> island_;

    std::vector<CollidePair> enters_;
    std::vector<CollidePair> exits_;

    std::vector<Collider*> detect_collider_result_;
    std::vector<int64_t> detect_actor_result_;
};

}
}
