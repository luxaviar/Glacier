#pragma once

#include <vector>
#include "geometry/aabb.h"
#include "Physics/Collision/HitResult.h"
#include "Physics/Collision/Narrowphase/MinkowskiSum.h"
#include "Physics/Collision/ContactPoint.h"
#include "Physics/Collision/CollidePair.h"
#include "Physics/CollisionFilter.h"

namespace glacier {

class Rigidbody;
class Collider;
class Gizmos;

namespace physics {

class World;
class BoardPhaseDetector;
class NarrowPhaseDetector;

class CollisionSystem : private Uncopyable {
public:
    CollisionSystem(std::unique_ptr<BoardPhaseDetector>&& boardDetector, 
        std::unique_ptr<NarrowPhaseDetector>&& narrowDetector);
    ~CollisionSystem();

    void AddCollider(Collider* collider);
    void RemoveCollider(Collider* collider);

    void Update(Rigidbody* body);

    std::vector<CollidePair>& Step(std::vector<CollidePair>& collision_enter, std::vector<CollidePair>& collision_exit);
    void Detect();

    RayHitResult RayCast(const Ray& ray, float max, uint32_t layer_mask, bool query_sensor);
    bool Detect(const AABB& aabb, std::vector<Collider*>& result, const CollisionFilter* filter);
    bool Detect(Collider* s, std::vector<Collider*>& result, const CollisionFilter* filter);
    void Clear();

    void OnDrawGizmos(bool draw_bvh);

    //const BoardPhaseDetector* boardphase_detector() const { return boardphase_detector_; }

private:
    void Minus(const std::vector<CollidePair>& a, const std::vector<CollidePair>& b, std::vector<CollidePair>& result) const;

    std::unique_ptr<BoardPhaseDetector> boardphase_detector_;
    std::unique_ptr<NarrowPhaseDetector> narrowphase_detector_;

    Simplex simplex_;
    ContactPoint contact_;

    std::vector<CollidePair> board_result_;
    std::vector<Collider*> board_query_result_;

    std::vector<CollidePair> collide_list_;
    std::vector<CollidePair> last_collide_list_;
};

}
}
