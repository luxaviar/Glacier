#include "CollisionSystem.h"
#include "Physics/Collision/Boardphase/BoardphaseDetector.h"
#include "Physics/Collision/Narrowphase/NarrowphaseDetector.h"
#include "Physics/Collider/Collider.h"
#include "Physics/Dynamic/Rigidbody.h"

namespace glacier {
namespace physics {

CollisionSystem::CollisionSystem(std::unique_ptr<BoardPhaseDetector>&& boardDetector,
        std::unique_ptr<NarrowPhaseDetector>&& narrowDetector) :
    boardphase_detector_(std::move(boardDetector)),
    narrowphase_detector_(std::move(narrowDetector))
{
}

CollisionSystem::~CollisionSystem() {
    
}

void CollisionSystem::AddCollider(Collider* collider) {
    boardphase_detector_->AddCollider(collider);
}

void CollisionSystem::RemoveCollider(Collider* collider) {
    for (size_t i = last_collide_list_.size(); i > 0; --i) {
        CollidePair& info = last_collide_list_[i - 1];
        Collider* a = info.first;
        Collider* b = info.second;
        if (a == collider || b == collider) {
            if (info.sensor) {
                a->OnSensorExit(b);
                b->OnSensorExit(a);
            } else {
                ContactPoint& ci = info.contact;
                a->OnCollisionExit(ContactInfo(true, b, ci.pointA, ci.pointB, ci.normal, ci.penetration));
                b->OnCollisionExit(ContactInfo(false, a, ci.pointB, ci.pointA, -ci.normal, ci.penetration));
            }

            last_collide_list_.erase(last_collide_list_.begin() + i - 1);
        }
    }

    boardphase_detector_->RemoveCollider(collider);
}

void CollisionSystem::Update(Rigidbody* body) {
    boardphase_detector_->UpdateBody(body);
}

std::vector<CollidePair>& CollisionSystem::Step(std::vector<CollidePair>& collision_enter, std::vector<CollidePair>& collision_exit) {
    Detect();

    Minus(collide_list_, last_collide_list_, collision_enter);
    Minus(last_collide_list_, collide_list_, collision_exit);

    std::swap(collide_list_, last_collide_list_);

    return last_collide_list_;
}

void CollisionSystem::Detect() {
    board_result_.clear();
    boardphase_detector_->Detect(board_result_);

    collide_list_.clear();
    for (size_t i = 0; i < board_result_.size(); ++i) {
        CollidePair& cp = board_result_[i];
        Collider* a = cp.first;
        Collider* b = cp.second;

        //simplex_.num = 0;
        bool hit = narrowphase_detector_->Detect(a, b, simplex_);
        if (hit) {
            if (a->is_contactable() && b->is_contactable()) {
                bool contat = narrowphase_detector_->Solve(a, b, simplex_, contact_);
                if (contat) {
                    collide_list_.emplace_back(a, b, contact_, false);
                } else {
                    //Debug.Log("Can't find contact manifold");
                }
            }
            else {
                collide_list_.push_back(cp);
            }
        }
    }
}

RayHitResult CollisionSystem::RayCast(const Ray& ray, float max, uint32_t layer_mask, bool query_sensor) {
    return boardphase_detector_->RayCast(ray, max, layer_mask, query_sensor);
}

bool CollisionSystem::Detect(const AABB& aabb, std::vector<Collider*>& result, const CollisionFilter* filter) {
    board_query_result_.clear();
    if (!boardphase_detector_->Detect(aabb, board_query_result_, filter)) {
        return false;
    }

    bool collide = false;
    for (auto collider : board_query_result_) {
        bool hit = narrowphase_detector_->Detect(aabb, collider, simplex_);
        if (hit) {
            result.push_back(collider);
            collide = true;
        }
    }

    return collide;
}

bool CollisionSystem::Detect(Collider* s, std::vector<Collider*>& result, const CollisionFilter* filter) {
    if (!s) return false;

    board_query_result_.clear();
    if (!boardphase_detector_->Detect(s->bounds(), board_query_result_, filter)) {
        return false;
    }

    bool collide = false;
    for (auto collider : board_query_result_) {
        bool hit = narrowphase_detector_->Detect(s, collider, simplex_);
        if (hit) {
            result.push_back(collider);
            collide = true;
        }
    }

    return collide;
}

void CollisionSystem::Clear() {
    boardphase_detector_->Clear();
    narrowphase_detector_->Clear();

    collide_list_.clear();
    last_collide_list_.clear();
}

void CollisionSystem::Minus(const std::vector<CollidePair>& a, const std::vector<CollidePair>& b, 
    std::vector<CollidePair>& result) const 
{
    for (size_t i = 0; i < a.size(); ++i) {
        const CollidePair& entry = a[i];
        bool have = false;
        for (size_t j = 0; j < b.size(); ++j) {
            if (b[j].Equals(entry)) {
                have = true;
                break;
            }
        }

        if (!have) {
            result.push_back(entry);
        }
    }
}

void CollisionSystem::OnDrawGizmos(bool draw_bvh) {
    boardphase_detector_->OnDrawGizmos(draw_bvh);
}

}
}

