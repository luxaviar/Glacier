#pragma once

#include <stdint.h>
#include <assert.h>
#include <queue>
#include <functional>
#include "Common/Freelist.h"
#include "Geometry/Aabb.h"

namespace glacier {

template<typename T>
struct BvhNode {
    int height;
    AABB bounds; //fat aabb
    T data;
    BvhNode<T>* parent;
    BvhNode<T>* left;
    BvhNode<T>* right;
    uint64_t epoch;

    BvhNode(T v) {
        height = 0;
        data = v;
        parent = nullptr;
        left = nullptr;
        right = nullptr;
    }

    BvhNode<T>* Sibling() const {
        if (parent->left == this) return parent->right;
        return parent->left;
    }

    void SetLeaf(BvhNode<T>* left, BvhNode<T>* right) {
        this->left = left;
        this->right = right;

        left->parent = this;
        right->parent = this;

        bounds = AABB::Union(left->bounds, right->bounds);
    }

    void UpdateAABB() {
        if (!IsLeaf()) {
            bounds = AABB::Union(left->bounds, right->bounds);
        }
    }

    bool Intersects(const AABB& other) const {
        return bounds.Intersects(other);
    }

    bool IsLeaf() const { return left == nullptr && right == nullptr; }

    void Reset(T v) {
        height = 0;
        data = v;
        parent = nullptr;
        left = nullptr;
        right = nullptr;
    }
};

template<typename T>
class BvhTree {
public:
    using NodeType = BvhNode<T>;

    using HitPair = std::pair<T, T>;
    using HitFilter = std::function<bool(const NodeType*, const NodeType*)>;
    using OnHitDelegate = std::function<void(T,T)>;

    using OverlayFilter = std::function<bool(const NodeType*)>;
    using RayHitDelegate = std::function<bool(const NodeType*, const Ray&, float, float&)>;

    struct RayHitResult {
        bool hit = false;
        float t = 0.0f;
        T data;
    };

    BvhTree(size_t capacity, float margin = 0.1f, float predict_multipler = 2.0f) :
        margin_(margin),
        predict_multipler_(predict_multipler),
        node_allocator_(capacity)
    {
    }

    uint64_t epoch() const { return epoch_; }

    NodeType* AddLeaf(T v, const AABB& bounds) {
        NodeType* node = node_allocator_.Acquire(v);

        if (margin_ > 0.0f) {
            node->bounds = bounds.Expand(margin_);
        }
        else {
            node->bounds = bounds;
        }
        
        InsertLeaf(node, true);

        return node;
    }

    void RemoveLeaf(NodeType* node) {
        if (!node) return;

        DeleteLeaf(node);
        FreeNode(node);
    }

    bool UpdateLeaf(NodeType* node, const AABB& bounds, const Vector3& move_hint, bool is_static) {
        assert(node && node->IsLeaf());

        if (node->bounds.Contains(bounds)) {
            return false;
        }

        DeleteLeaf(node);
        
        if (is_static) {
            node->bounds = bounds;
            InsertLeaf(node);

            return true;
        }

        AABB fat_aabb = bounds.Expand(margin_);
        //Vec3f movehint = bounds.Center() - node->bounds.Center();
        Vec3f predict = move_hint * predict_multipler_;

        if (predict.x > 0) {
            fat_aabb.max.x += predict.x;
        }
        else {
            fat_aabb.min.x -= predict.x;
        }

        if (predict.y > 0) {
            fat_aabb.max.y += predict.y;
        }
        else {
            fat_aabb.min.y -= predict.y;
        }

        if (predict.z > 0) {
            fat_aabb.max.z += predict.z;
        }
        else {
            fat_aabb.min.z -= predict.z;
        }

        node->bounds = fat_aabb;
        InsertLeaf(node);

        return true;
    }

    NodeType* Balance(NodeType* A) {
        if (A->IsLeaf() || A->height < 2) return A;

        NodeType* B = A->left;
        NodeType* C = A->right;
        int balance = C->height - B->height;
        if (balance > 1) {
            NodeType* F = C->left;
            NodeType* G = C->right;

            C->left = A;
            C->parent = A->parent;
            A->parent = C;

            if (C->parent != nullptr) {
                if (C->parent->left == A) {
                    C->parent->left = C;
                }
                else {
                    C->parent->right = C;
                }
            }
            else {
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
            }
            else {
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
            NodeType* D = B->left;
            NodeType* E = B->right;

            B->left = A;
            B->parent = A->parent;
            A->parent = B;

            if (B->parent != nullptr) {
                if (B->parent->left == A) {
                    B->parent->left = B;
                }
                else {
                    B->parent->right = B;
                }
            }
            else {
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
            }
            else {
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

    RayHitResult QueryByRay(const Ray& ray, float max, const RayHitDelegate& hit_func) {
        RayHitResult res;

        std::queue<NodeType*> queue;
        if (root_ != nullptr) {
            queue.push(root_);
        }

        while (!queue.empty()) {
            auto node = queue.front();
            queue.pop();
            bool hit = false;
            float t;

            hit = node->bounds.Intersects(ray, max, t);

            if (hit) {
                if (res.hit && t > res.t) {
                    continue;
                }

                if (node->IsLeaf()) {
                    if (hit_func(node, ray, max, t) &&
                        (!res.hit || t < res.t))
                    {
                        res.t = t;
                        res.data = node->data;
                        res.hit = true;
                    }
                }
                else {
                    queue.push(node->left);
                    queue.push(node->right);
                }
            }
        }

        return res;
    }

    bool QueryByBounds(const AABB& aabb, std::vector<T>& result, const OverlayFilter& filter) {
        NodeType* target = root_;
        bool hit = false;

        while (target) {
            if (target->IsLeaf()) {
                if (filter(target) && target->Intersects(aabb)) {
                    result.push_back(target->data);
                    hit = true;
                }
            }
            else {
                if (target->Intersects(aabb)) {
                    target = target->left;
                    continue;
                }
            }

            bool found_next = false;
            while (target->parent != nullptr) {
                if (target == target->parent->left) {
                    target = target->parent->right;
                    found_next = true;
                    break;
                }

                target = target->parent;
            }

            if (!found_next) break;
        }

        return hit;
    }

    void QueryCollision(const HitFilter& filter, const OnHitDelegate& on_hit) {
        ++query_epoch_;

        detect_result_.clear();
        for (auto node : leafs_) {
            QueryByNode(node, detect_result_, filter);

            for (auto v : detect_result_) {
                on_hit(node->data, v);
            }
            detect_result_.clear();
            node->epoch = query_epoch_;
        }
    }

    bool QueryByNode(NodeType* node, std::vector<T>& result, const HitFilter& filter) {
        NodeType* target = root_;
        const auto& aabb = node->bounds;
        bool hit = false;

        while (target) {
            if (target->IsLeaf()) {
                if (target != node &&
                    target->epoch != query_epoch_ &&
                    filter(target, node) && target->Intersects(aabb))
                {
                    result.push_back(target->data);
                    hit = true;
                }
            }
            else {
                if (target->Intersects(aabb)) {
                    target = target->left;
                    continue;
                }
            }

            bool found_next = false;
            while (target->parent != nullptr) {
                if (target == target->parent->left) {
                    target = target->parent->right;
                    found_next = true;
                    break;
                }

                target = target->parent;
            }

            if (!found_next) break;
        }

        return hit;
    }

    void Reset() {
        root_ = nullptr;
        leafs_.clear();
        node_allocator_.Reset();
    }

protected:
    void FreeNode(NodeType* node) {
        auto it = std::find(leafs_.begin(), leafs_.end(), node);
        if (it != leafs_.end()) leafs_.erase(it);

        node_allocator_.Release(node);
    }

    void ReBalance(NodeType* node) {
        while (node != nullptr) {
            //re balance
            node = Balance(node);

            node->height = math::Max(node->left->height, node->right->height) + 1;
            node->UpdateAABB();
            node = node->parent;
        }
    }

    void DeleteLeaf(NodeType* node) {
        ++epoch_;

        assert(node->IsLeaf());

        NodeType* parent = node->parent;
        if (parent == nullptr) {
            assert(node == root_);
            root_ = nullptr;
            return;
        }

        NodeType* grandpa = parent->parent;
        NodeType* sibling = node->Sibling();

        if (grandpa == nullptr) {
            root_ = sibling;
            sibling->parent = nullptr;
            FreeNode(parent);
        }
        else {
            sibling->parent = grandpa;
            if (grandpa->left == parent) {
                grandpa->left = sibling;
            }
            else {
                grandpa->right = sibling;
            }

            FreeNode(parent);
            ReBalance(grandpa);
        }
    }

    void InsertLeaf(NodeType* node, bool is_new = false) {
        ++epoch_;

        if (is_new) {
            leafs_.push_back(node);
        }

        if (!root_) {
            root_ = node;
            return;
        }

        NodeType* parent = root_;
        AABB& new_aabb = node->bounds;
        while (!parent->IsLeaf()) {
            AABB& parentAabb = parent->bounds;
            float mergedVol = AABB::Union(parentAabb, new_aabb).Volume();

            float costS = 2.0f * mergedVol;
            float costI = 2.0f * (mergedVol - parentAabb.Volume());

            float costLeft;
            if (parent->left->IsLeaf()) {
                costLeft = AABB::Union(parent->left->bounds, new_aabb).Volume() + costI;
            }
            else {
                costLeft = AABB::Union(parent->left->bounds, new_aabb).Volume() + costI - parent->left->bounds.Volume();
            }

            float costRight;
            if (parent->right->IsLeaf()) {
                costRight = AABB::Union(parent->right->bounds, new_aabb).Volume() + costI;
            }
            else {
                costRight = AABB::Union(parent->right->bounds, new_aabb).Volume() + costI - parent->right->bounds.Volume();
            }

            if (costS < costLeft && costS < costRight) break;

            if (costLeft < costRight) {
                parent = parent->left;
            }
            else {
                parent = parent->right;
            }
        }

        NodeType* grandpa = parent->parent;
        NodeType* sibling = parent;
        parent = node_allocator_.Acquire(nullptr);
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

        ReBalance(parent);
    }

protected:
    float margin_;
    float predict_multipler_;

    uint64_t epoch_ = 0;
    uint64_t query_epoch_ = 0;
    NodeType* root_ = nullptr;

    std::vector<NodeType*> leafs_;
    std::vector<T> detect_result_;

    FreeList<NodeType> node_allocator_;
};

}
