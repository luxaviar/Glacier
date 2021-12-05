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

uint32_t BvhNode::id_counter_ = 0;

BvhNode::BvhNode(Collider* collider, float margin) {
    Reset(collider, margin);
}

BvhNode::~BvhNode() {
    //Reset();
}

void BvhNode::Reset(Collider* co, float margin) {
    height = 0;
    collider = co;
    parent = nullptr;
    left = nullptr;
    right = nullptr;
    id = ++id_counter_;
    epoch = 0;
    moving = false;
    
    if (collider != nullptr) {
        aabb = collider->bounds().Expand(margin);
        collider->node_ = this;
    }
}

BvhNode* BvhNode::Sibling() const {
    if (parent->left == this) return parent->right;
    return parent->left;
}

void BvhNode::SetLeaf(BvhNode* left, BvhNode* right) {
    this->left = left;
    this->right = right;

    left->parent = this;
    right->parent = this;

    aabb = AABB::Union(left->aabb, right->aabb);
}

void BvhNode::UpdateAABB() {
    if (!is_leaf()) {
        aabb = AABB::Union(left->aabb, right->aabb);
    }
}

bool BvhNode::Intersects(const AABB& other) const {
    return aabb.Intersects(other);
}

void BvhNode::OnDrawGizmos(bool draw_bvh) {
    auto& gizmo = *render::Gizmos::Instance();
    if (is_leaf()) {
        auto rigidbody = collider->rigidbody();
        if (rigidbody && rigidbody->is_dynamic() && !rigidbody->asleep()) {
            gizmo.SetColor(Color::kCyan);
        } else {
            gizmo.SetColor(Color::kYellow);
        }
        collider->OnDrawSelectedGizmos();
    }
    else if (draw_bvh) {
        gizmo.SetColor(Color::kGreen);
        gizmo.DrawCube(aabb.Center(), aabb.Extent());
    }

    if (left) left->OnDrawGizmos(draw_bvh);
    if (right) right->OnDrawGizmos(draw_bvh);
}

DynamicBvh::DynamicBvh(CollisionFilter* filter, size_t capacity, float margin, float predict_multipler) :
    margin_(margin),
    predict_multipler_(predict_multipler),
    epoch_(1),
    query_epoch_(1),
    root_(nullptr),
    filter_(filter),
    node_allocator_(capacity) { //TODO: check capacity!
}

DynamicBvh::~DynamicBvh() {
}

void DynamicBvh::FreeNode(BvhNode* node) {
    auto it = std::find(leafs_.begin(), leafs_.end(), node);
    if (it != leafs_.end()) leafs_.erase(it);

    node_allocator_.Release(node);
}

void DynamicBvh::AddCollider(Collider* collider) {
    assert(collider->node_ == nullptr);

    BvhNode* node = node_allocator_.Acquire(collider, margin_);
    InsertLeaf(node, true);
}

void DynamicBvh::RemoveCollider(Collider* collider) {
    BvhNode* node = collider->node_;
    if (node) {
        DeleteLeaf(node);
        FreeNode(node);
        collider->node_ = nullptr;
    }
}

void DynamicBvh::UpdateBody(Rigidbody* body) {
    for (auto collider : body->colliders_) {
        if (collider->node_) {
            UpdateLeaf(collider->node_);
        }
    }
}

bool DynamicBvh::UpdateLeaf(BvhNode* node) {
    assert(node && node->is_leaf());

    auto collider = node->collider;
    const AABB& bounds = collider->bounds();

    if (node->aabb.Contains(bounds)) {
        return false;
    }

    DeleteLeaf(node);

    //Rigidbody* body = collider->rigidbody_;
    if (collider->is_static()) {
        node->aabb = bounds;
        InsertLeaf(node);

        return true;
    }

    AABB flatAabb = bounds.Expand(margin_);
    Vec3f& movehint = collider->rigidbody()->displacement_;
    Vec3f predict = movehint * predict_multipler_;

    if (predict.x > 0) {
        flatAabb.max.x += predict.x;
    }
    else {
        flatAabb.min.x -= predict.x;
    }

    if (predict.y > 0) {
        flatAabb.max.y += predict.y;
    }
    else {
        flatAabb.min.y -= predict.y;
    }

    if (predict.z > 0) {
        flatAabb.max.z += predict.z;
    }
    else {
        flatAabb.min.z -= predict.z;
    }

    node->aabb = flatAabb;
    InsertLeaf(node);

    return true;
}

BvhNode* DynamicBvh::Balance(BvhNode* A) {
    if (A->is_leaf() || A->height < 2) return A;

    BvhNode* B = A->left;
    BvhNode* C = A->right;
    int balance = C->height - B->height;
    if (balance > 1) {
        BvhNode* F = C->left;
        BvhNode* G = C->right;

        C->left = A;
        C->parent = A->parent;
        A->parent = C;

        if (C->parent != nullptr) {
            if (C->parent->left == A) {
                C->parent->left = C;
            } else {
                C->parent->right = C;
            }
        } else {
            root_ = C;
        }

        if (F->height > G->height) {
            C->right = F;
            A->right = G;
            G->parent = A;
            A->UpdateAABB();
            C->UpdateAABB();

            A->height = 1 + math::Max(B->height, C->height);
            C->height = 1 + math::Max(A->height, F->height);
        } else {
            C->right = G;
            A->right = F;
            F->parent = A;
            A->UpdateAABB();
            C->UpdateAABB();

            A->height = 1 + math::Max(B->height, F->height);
            C->height = 1 + math::Max(A->height, G->height);
        }

        return C;
    }

    if (balance < -1) {
        BvhNode* D = B->left;
        BvhNode* E = B->right;

        B->left = A;
        B->parent = A->parent;
        A->parent = B;

        if (B->parent != nullptr) {
            if (B->parent->left == A) {
                B->parent->left = B;
            } else {
                B->parent->right = B;
            }
        } else {
            root_ = B;
        }

        if (D->height > E->height) {
            B->right = D;
            A->left = E;
            E->parent = A;
            A->UpdateAABB();
            B->UpdateAABB();

            A->height = 1 + math::Max(C->height, E->height);
            B->height = 1 + math::Max(A->height, D->height);
        } else {
            B->right = E;
            A->left = D;
            D->parent = A;
            A->UpdateAABB();
            B->UpdateAABB();

            A->height = 1 + math::Max(C->height, D->height);
            B->height = 1 + math::Max(A->height, E->height);
        }

        return B;
    }

    return A;
}

void DynamicBvh::RefrshNode(BvhNode* node) {
    while (node != nullptr) {
        //re balance
        node = Balance(node);

        node->height = math::Max(node->left->height, node->right->height) + 1;
        node->UpdateAABB();
        node = node->parent;
    }
}

void DynamicBvh::DeleteLeaf(BvhNode* node) {
    ++epoch_;

    assert(node->is_leaf());

    BvhNode* parent = node->parent;
    if (parent == nullptr) {
        assert(node == root_);
        //if (node != root_) throw new InvalidOperationException("remove root fail");
        root_ = nullptr;
        return;
    }

    BvhNode* grandpa = parent->parent;
    BvhNode* sibling = node->Sibling();

    if (grandpa == nullptr) {
        root_ = sibling;
        sibling->parent = nullptr;
        FreeNode(parent);
    } else {
        sibling->parent = grandpa;
        if (grandpa->left == parent) {
            grandpa->left = sibling;
        } else {
            grandpa->right = sibling;
        }

        FreeNode(parent);
        RefrshNode(grandpa);
    }
}

void DynamicBvh::InsertLeaf(BvhNode* node, bool isNew) {
    ++epoch_;

    if (isNew) {
        leafs_.push_back(node);
    }

    node->moving = true;

    if (root_ == nullptr) {
        root_ = node;
        return;
    }

    BvhNode* parent = root_;
    AABB& newAabb = node->aabb;
    while (!parent->is_leaf()) {
        AABB& parentAabb = parent->aabb;
        float mergedVol = AABB::Union(parentAabb, newAabb).Volume();

        float costS = 2.0f * mergedVol;
        float costI = 2.0f * (mergedVol - parentAabb.Volume());

        float costLeft;
        if (parent->left->is_leaf()) {
            costLeft = AABB::Union(parent->left->aabb, newAabb).Volume() + costI;
        }
        else {
            costLeft = AABB::Union(parent->left->aabb, newAabb).Volume() + costI - parent->left->aabb.Volume();
        }

        float costRight;
        if (parent->right->is_leaf()) {
            costRight = AABB::Union(parent->right->aabb, newAabb).Volume() + costI;
        }
        else {
            costRight = AABB::Union(parent->right->aabb, newAabb).Volume() + costI - parent->right->aabb.Volume();
        }

        if (costS < costLeft && costS < costRight) break;

        if (costLeft < costRight) {
            parent = parent->left;
        }
        else {
            parent = parent->right;
        }
    }

    BvhNode* grandpa = parent->parent;
    BvhNode* sibling = parent;
    parent = node_allocator_.Acquire(nullptr, margin_);
    if (grandpa == nullptr) {
        root_ = parent;
    }
    else {
        if (grandpa->left == sibling) {
            grandpa->left = parent;
        }
        else {
            grandpa->right = parent;
        }
    }
    parent->parent = grandpa;
    parent->SetLeaf(node, sibling);

    RefrshNode(parent);
}

RayHitResult DynamicBvh::RayCast(const Ray &ray, float max, uint32_t layer_mask, bool query_sensor) {
    RayHitResult res;
    res.hit = false;
    res.t = 0.0f;

    //int total = 0;
    std::queue<BvhNode*> queue;
    if (root_ != nullptr) {
        queue.push(root_);
        //total += 1;
    }

    while (!queue.empty()) {
        auto node = queue.front();
        queue.pop();
        bool hit = false;
        float t;

        if (node->is_leaf()) {
            hit = node->collider->Intersects(ray, max, t);
        } else {
            hit = node->aabb.Intersects(ray, max, t);
        }

        if (hit) {
            if (res.hit && t > res.t) {
                continue;
            }

            if (node->is_leaf()) {
                auto collider = node->collider;
                if ((collider->layer() & layer_mask) &&
                    (query_sensor || !collider->is_sensor()) &&
                    (!res.hit || t < res.t)) 
                {
                    res.t = t;
                    res.collider = collider;
                }
                res.hit = true;
            } else {
                queue.push(node->left);
                queue.push(node->right);
                //total += 2;
            }
        }
    }

    return res;
}

bool DynamicBvh::Detect(const AABB &aabb, std::vector<Collider*>& result, const CollisionFilter* filter) {
    BvhNode* target = root_;
    bool hit = false;

    while (target != nullptr) {
        if (target->is_leaf()) {
            if ((!filter || filter->CanCollide(target->collider)) &&
                    target->Intersects(aabb)) {
                result.push_back(target->collider);
                hit = true;
            }
        } else {
            if (target->Intersects(aabb)) {
                target = target->left;
                continue;
            }
        }

        bool foundNext = false;
        while (target->parent != nullptr) {
            if (target == target->parent->left) {
                target = target->parent->right;
                foundNext = true;
                break;
            }

            target = target->parent;
        }

        if (!foundNext) break;
    }

    return hit;
}

void DynamicBvh::Detect(std::vector<CollidePair>& result) {
    ++query_epoch_;
    if (query_epoch_ == 0) query_epoch_ = 1;
    detect_result_.clear();
    for (auto node : leafs_) {
        if (!node->moving) continue;

        Detect(node->aabb, node, detect_result_);

        for (auto ft : detect_result_) {
            result.emplace_back(node->collider, ft);
        }
        detect_result_.clear();
        node->epoch = query_epoch_;
    }
}

bool DynamicBvh::CanCollide(Collider* a, Collider* b) {
    if (!filter_->CanCollide(a, b)) {
        return false;
    }

    return true;
}

bool DynamicBvh::Detect(const AABB& aabb, BvhNode* node, std::vector<Collider*>& result) {
    BvhNode* target = root_;
    bool hit = false;

    while (target != nullptr) {
        if (target->is_leaf()) {
            Collider* collider1 = target->collider;
            Collider* collider2 = node->collider;

            if (target->epoch != query_epoch_ &&
                    target != node &&
                    collider1 != collider2 &&
                    CanCollide(collider1, collider2)) {
                if (target->Intersects(aabb)) {
                    result.push_back(target->collider);
                    hit = true;
                }
            }
        } else {
            if (target->Intersects(aabb)) {
                target = target->left;
                continue;
            }
        }

        bool foundNext = false;
        while (target->parent != nullptr) {
            if (target == target->parent->left) {
                target = target->parent->right;
                foundNext = true;
                break;
            }

            target = target->parent;
        }

        if (!foundNext) break;
    }

    return hit;
}

void DynamicBvh::Query(const AABB& aabb, std::vector<Collider*>& result) {
    ++query_epoch_;
    if (query_epoch_ == 0) query_epoch_ = 1;
    Detect(aabb, nullptr, result);
}

void DynamicBvh::Clear() {
    root_ = nullptr;
    leafs_.clear();
    node_allocator_.Reset();
}

void DynamicBvh::OnDrawGizmos(bool draw_bvh) {
    if (root_ == nullptr) return;

    root_->OnDrawGizmos(draw_bvh);
}

}
}

