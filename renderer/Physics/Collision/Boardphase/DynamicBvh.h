#pragma once

#include <stdint.h>
#include <assert.h>
#include <queue>
#include "Common/Freelist.h"
#include "Geometry/Aabb.h"
#include "Physics/Collision/Boardphase/BoardphaseDetector.h"
#include "Physics/Collision/CollidePair.h"
#include "Physics/Collision/HitResult.h"

namespace glacier {

class Rigidbody;
class Collider;
class Gizmos;

namespace physics {

class CollisionFilter;

struct BvhNode {
    static uint32_t id_counter_;
    //for balance
    int height;
    //fat aabb
    AABB aabb;
    Collider* collider;
    BvhNode* parent;
    BvhNode* left;
    BvhNode* right;

    uint32_t id;

    //for collision detect
    uint32_t epoch;
    //uint32_t collideEpoch = 0;
    bool moving = false;

    BvhNode(Collider* ft, float margin);
    ~BvhNode();

    void Reset(Collider* ft, float margin);
    //void Reset();
    BvhNode* Sibling() const;
    void SetLeaf(BvhNode* left, BvhNode* right);
    void UpdateAABB();
    bool Intersects(const AABB& other) const;

    bool is_leaf() const { return left == nullptr && right == nullptr; }

    void OnDrawGizmos(bool draw_bvh);
};

class DynamicBvh : public BoardPhaseDetector {
public:
    DynamicBvh(CollisionFilter* filter, size_t capacity, float margin = 0.1f, float predictMultipler = 2.0f);
    ~DynamicBvh();

    uint32_t epoch() const override { return epoch_; }

    void AddCollider(Collider* collider) override;
    void RemoveCollider(Collider* body) override;

    void UpdateBody(Rigidbody* body) override;

    bool Detect(const AABB& aabb, std::vector<Collider*>& result, const CollisionFilter* filter) override;
    RayHitResult RayCast(const Ray& ray, float max, uint32_t layer_mask, bool query_sensor) override;

    void Detect(std::vector<CollidePair>& result) override;
    void Query(const AABB& aabb, std::vector<Collider*>& result);

    void Clear() override;

    void OnDrawGizmos(bool draw_bvh) override;

private:
    bool CanCollide(Collider* a, Collider* b);

    BvhNode* Balance(BvhNode* A);
    void FreeNode(BvhNode* node);
    void RefrshNode(BvhNode* node);

    bool UpdateLeaf(BvhNode* node);
    void DeleteLeaf(BvhNode* node);
    void InsertLeaf(BvhNode* node, bool isNew=false);
    bool Detect(const AABB& aabb, BvhNode* node, std::vector<Collider*>& result);

private:
    float margin_;
    float predict_multipler_;

    uint32_t epoch_;
    uint32_t query_epoch_;// = 0;
    BvhNode* root_;
    CollisionFilter* filter_;

    std::vector<BvhNode*> leafs_;
    std::vector<Collider*> detect_result_;
    FreeList<BvhNode> node_allocator_;
};

}
}
