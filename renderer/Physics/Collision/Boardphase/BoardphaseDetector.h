#pragma once

#include <vector>
#include "Common/Uncopyable.h"
#include "Physics/Collision/CollidePair.h"
#include "Physics/Collision/HitResult.h"
#include "Physics/CollisionFilter.h"

namespace glacier {

class Rigidbody;
class Collider;
class Gizmos;

namespace physics {

class BoardPhaseDetector : private Uncopyable {
public:
    virtual ~BoardPhaseDetector() {};

    virtual void AddCollider(Collider* body) = 0;
    virtual void RemoveCollider(Collider* body) = 0;
    virtual void UpdateBody(Rigidbody* body) = 0;

    virtual bool Detect(const AABB& aabb, std::vector<Collider*>& result, const CollisionFilter* filter) = 0;
    virtual RayHitResult RayCast(const Ray& ray, float max, uint32_t layer_mask, bool query_sensor) = 0;

    virtual void Detect(std::vector<CollidePair>& result) = 0;

    virtual void Clear() {};
    virtual uint32_t GetEpoch() const { return 0; }

    virtual void OnDrawGizmos(bool draw_bvh) = 0;
};

}
}
