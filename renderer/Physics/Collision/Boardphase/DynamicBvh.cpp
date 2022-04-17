#include "DynamicBvh.h"
#include <stdint.h>
#include <assert.h>
#include <queue>
#include <algorithm>
#include "Common/Freelist.h"
#include "Physics/Collider/Collider.h"
#include "Physics/World.h"
#include "Physics/Types.h"
#include "Physics/Dynamic/Rigidbody.h"
#include "Physics/CollisionFilter.h"
#include "Render/Editor/Gizmos.h"

namespace glacier {
namespace physics {

DynamicBvh::DynamicBvh(CollisionFilter* filter, size_t capacity, float margin, float predict_multipler) :
    TreeType(capacity, margin, predict_multipler),
    filter_(filter)
{
}

DynamicBvh::~DynamicBvh() {
}

void DynamicBvh::AddCollider(Collider* collider) {
    assert(collider->node_ == nullptr);

    auto& bounds = collider->bounds();
    auto node = AddLeaf(collider, bounds);
    collider->node_ = node;
}

void DynamicBvh::RemoveCollider(Collider* collider) {
    auto node = collider->node_;
    if (node) {
        RemoveLeaf(node);
        collider->node_ = nullptr;
    }
}

void DynamicBvh::UpdateBody(Rigidbody* body) {
    for (auto collider : body->colliders_) {
        if (collider->node_) {
            UpdateLeaf(collider->node_, collider->bounds(), body->displacement_, collider->is_static());
        }
    }
}

physics::RayHitResult DynamicBvh::RayCast(const Ray &ray, float max, uint32_t layer_mask, bool query_sensor) {
    auto result = QueryByRay(ray, max, [layer_mask, query_sensor](const NodeType* node, const Ray& ray, float max, float& t) {
        auto collider = node->data;
        if (!collider->Intersects(ray, max, t)) {
            return false;
        }

        if ((collider->layer() & layer_mask) &&
            (query_sensor || !collider->is_sensor()))
        {
            return true;
        }
        return false;
    });

    physics::RayHitResult res;
    res.collider = result.data;
    res.hit = result.hit;
    res.t = result.t;

    return res;
}

bool DynamicBvh::Detect(const AABB &aabb, std::vector<Collider*>& result, const CollisionFilter* filter) {
    bool hit = QueryByBounds(aabb, result, [filter](const NodeType* node) {
        return !filter || filter->CanCollide(node->data);
    });

    return hit;
}

void DynamicBvh::Detect(std::vector<CollidePair>& result) {
    QueryCollision(
        [this](const NodeType* a, const NodeType* b) {
            return !filter_ || filter_->CanCollide(a->data, b->data);
        },
        [&result](Collider* a, Collider* b) {
            result.emplace_back(a, b);
        }
    );
}

void DynamicBvh::OnDrawGizmos(bool draw_bvh) {
    DrawGizmos(root_, draw_bvh);
}

void DynamicBvh::DrawGizmos(NodeType* node, bool draw_bvh) {
    if (node == nullptr) return;

    auto& gizmo = *render::Gizmos::Instance();
    if (node->IsLeaf()) {
        auto rigidbody = node->data->rigidbody();
        if (rigidbody && rigidbody->is_dynamic() && !rigidbody->asleep()) {
            gizmo.SetColor(Color::kCyan);
        }
        else {
            gizmo.SetColor(Color::kYellow);
        }
        node->data->OnDrawSelectedGizmos();
    }
    else if (draw_bvh) {
        gizmo.SetColor(Color::kGreen);
        gizmo.DrawCube(node->bounds.Center(), node->bounds.Extent());
    }

    DrawGizmos(node->left, draw_bvh);
    DrawGizmos(node->right, draw_bvh);
}

}
}
