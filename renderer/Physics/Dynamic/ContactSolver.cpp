#include "ContactSolver.h"
#include "Physics/Collision/CollidePair.h"
#include "Physics/Dynamic/Rigidbody.h"
#include "Physics/Collider/Collider.h"

namespace glacier {
namespace physics {

ContactSolver::ContactSolver(uint32_t maxIter) :
    inv_interval_(30.0f),
    max_iteration_(maxIter) {
}

ContactSolver::~ContactSolver() {
    manifolds_.clear();
}

void ContactSolver::ClearContact(Collider* collider) {
    if (collider->contacts_.empty()) return;

    //when ContactMainfold erase from map, the iterator invalidation will cause by dtor, so we need the swap
    std::vector<uint64_t> contact_list;
    std::swap(contact_list, collider->contacts_);

    for (auto key : contact_list) {
        auto it = manifolds_.find(key);
        assert(it != manifolds_.end());
        if (it != manifolds_.end()) {
            ContactManifold& mf = it->second;
            Collider* other = mf.GetOther(collider);
            if (other->rigidbody()) {
                other->rigidbody()->Awake();
            }

            manifolds_.erase(it);
        }
    }
}

void ContactSolver::Step(std::vector<CollidePair> &collideList) {
    UpdateContact();

    for (const auto &cp : collideList) {
        if (cp.sensor) continue;

        auto a = cp.first;
        auto b = cp.second;
        if ((!a->is_dynamic() && !b->is_dynamic()) ||
            a->is_sensor() || b->is_sensor()) {
            continue;
        }

        AddContactPoint(a, b, cp.contact);
    }
}

void ContactSolver::UpdateContact() {
    for (auto& it : manifolds_) {
        it.second.UpdateContact();
        if (it.second.count() == 0) {
            removes_.push_back(it.first);
        }
    }

    for (auto key : removes_) {
        auto it = manifolds_.find(key);
        manifolds_.erase(it);
    }

    removes_.clear();
}

void ContactSolver::AddContactPoint(Collider* a, Collider* b, const ContactPoint& ci) {
    uint64_t key = (uint64_t)a->id() | ((uint64_t)b->id() << 32);
    auto it = manifolds_.find(key);
    if (it != manifolds_.end()) {
        it->second.Add(ci);
    } else {
        manifolds_.emplace_hint(it, key, ContactManifold(a, b, key, ci));
        //manifolds_.emplace(key, a, b, key, ci);
    }
}

void ContactSolver::Solve(std::vector<ContactManifold*>& mfs, float warmstartRatio) const {
    for (auto& mf : mfs) {
        mf->WarmStart(warmstartRatio);
    }

    if (inv_interval_ > math::kEpsilon) {
        for (uint32_t i = 0; i < max_iteration_; ++i) {
            for (auto& mf : mfs) {
                mf->Solve(inv_interval_);
            }
        }
    }
}

ContactManifold* ContactSolver::Find(uint64_t key) const {
    auto it =  manifolds_.find(key);
    if (it != manifolds_.end()) {
        return const_cast<ContactManifold*>(&it->second);
    }

    return nullptr;
}

void ContactSolver::Clear() {
    manifolds_.clear();
}

void ContactSolver::OnDrawGizmos() {
    for (auto& entry : manifolds_) {
        auto& cont = entry.second;
        auto& list = cont.GetContacts();
        for (auto& c : list) {
            c.OnDrawGizmos();
        }
    }
}

}
}

