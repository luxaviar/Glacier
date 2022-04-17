#pragma once

#include <stdint.h>
#include <assert.h>
#include <queue>
#include "Common/Freelist.h"
#include "Geometry/Aabb.h"
#include "Physics/Collision/Boardphase/BoardphaseDetector.h"
#include "Physics/Collision/CollidePair.h"
#include "Physics/Collision/HitResult.h"
#include "Algorithm/Bvh.h"

namespace glacier {

class Rigidbody;
class Collider;
class Gizmos;

namespace physics {

class CollisionFilter;

class DynamicBvh : public BvhTree<Collider*>, public BoardPhaseDetector {
public:
    using TreeType = BvhTree<Collider*>;

    DynamicBvh(CollisionFilter* filter, size_t capacity, float margin = 0.1f, float predictMultipler = 2.0f);
    ~DynamicBvh();

    uint32_t GetEpoch() const override { return epoch(); }
    void Clear() override { Reset(); }

    void AddCollider(Collider* collider) override;
    void RemoveCollider(Collider* body) override;
    void UpdateBody(Rigidbody* body) override;

    bool Detect(const AABB& aabb, std::vector<Collider*>& result, const CollisionFilter* filter) override;
    void Detect(std::vector<CollidePair>& result) override;
    physics::RayHitResult RayCast(const Ray& ray, float max, uint32_t layer_mask, bool query_sensor) override;

    void OnDrawGizmos(bool draw_bvh) override;

private:
    void DrawGizmos(NodeType* node, bool draw_bvh);

    CollisionFilter* filter_;
};

}
}
