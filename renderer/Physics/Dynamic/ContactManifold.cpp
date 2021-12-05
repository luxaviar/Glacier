#include "ContactManifold.h"
#include <limits>
#include <algorithm>
#include "Geometry/Linesegment.h"
#include "Geometry/Triangle.h"
#include "Physics/Collider/Collider.h"
#include "Physics/Dynamic/Rigidbody.h"
#include "Physics/World.h"

namespace glacier {
namespace physics {

ContactManifold::ContactManifold(ContactManifold&& other) noexcept :
    a_(other.a_),
    b_(other.b_),
    friction_(other.friction_),
    restitution_(other.restitution_),
    island_ver_(other.island_ver_),
    key_(other.key_),
    contacts_(std::move(other.contacts_)) {

    for (auto& c : contacts_) {
        c.manifold = this;
    }

    other.a_ = nullptr;
    other.b_ = nullptr;
}

ContactManifold::ContactManifold(Collider* a, Collider* b, uint64_t key, const ContactPoint& ci) :
    a_(a),
    b_(b),
    island_ver_(0),
    key_(key)
{
    assert(!a->is_sensor());
    assert(!b->is_sensor());

    // Friction mixing. The idea is to allow a very low friction value to
    // drive down the mixing result. Example: anything slides on ice.
    friction_ = math::Sqrt(a->friction() * b->friction());

    // Restitution mixing. The idea is to use the maximum bounciness, so bouncy
    // objects will never not bounce during collisions.
    restitution_ = math::Max(a->restitution(), b->restitution());

    if (!a_->is_static()) {
        a_->AddContact(key);
    }

    if (!b_->is_static()) {
        b_->AddContact(key);
    }

    contacts_.emplace_back(this, ci);
}

ContactManifold::~ContactManifold() {
    if (a_) {
        a_->RemoveContact(key_);
    }

    if (b_) {
        b_->RemoveContact(key_);
    }
}

size_t ContactManifold::count() const {
    return contacts_.size();
}

uint64_t ContactManifold::key() const {
    return key_;
}

Collider* ContactManifold::GetOther(Collider* me) {
    if (me == a_) {
        return b_;
    }

    return a_;
}

void ContactManifold::UpdateContact() {
    if (!a_->is_contactable() || !b_->is_contactable()) {
        contacts_.clear();
        return;
    }

    // check if any existing contact is not valid any more
    for (size_t i = contacts_.size(); i > 0; --i) {
        Contact& contact = contacts_[i - 1];
        Vec3f localToGlobalA = a_->transform().ApplyTransform(contact.localPointA);
        Vec3f localToGlobalB = b_->transform().ApplyTransform(contact.localPointB);

        Vec3f rA = contact.globalPointA - localToGlobalA;
        Vec3f rB = contact.globalPointB - localToGlobalB;

        float persistent_threshold_sq = World::Instance()->persistent_threshold_sq();
        bool rACloseEnough = rA.MagnitudeSq() < persistent_threshold_sq;
        bool rBCloseEnough = rB.MagnitudeSq() < persistent_threshold_sq;

        // keep contact posize_t if the collision pair is
        // still colliding at this point, and the local
        // positions are not too far from the global
        // positions original acquired from collision detection
        if (rACloseEnough && rBCloseEnough) {
            // contact persistent, keep
            contact.persistent = true;
        } else {
            // contact invalid, remove
            contacts_.erase(contacts_.begin() + i - 1);
        }
    }
}

void ContactManifold::Add(const ContactPoint& ci) {
    // proximity check
    for (const auto& contact : contacts_) {
        Vec3f rA = ci.pointA - contact.pointA;
        Vec3f rB = ci.pointB - contact.pointB;

        float persistent_threshold_sq = World::Instance()->persistent_threshold_sq();
        bool rAFarEnough = rA.MagnitudeSq() > persistent_threshold_sq;
        bool rBFarEnough = rB.MagnitudeSq() > persistent_threshold_sq;

        //use old one;
        if (!rAFarEnough && !rBFarEnough) {
            //tmpCi_ = ci;
            return;
        }
    }

    contacts_.emplace_back(this, ci);

    if (contacts_.size() > 4) {
        Discard();
    }
}

void ContactManifold::Discard() {
    // find the deepest penetrating one
    size_t deepest;
    float penetration = std::numeric_limits<float>::lowest();
    for (size_t i = 0; i < contacts_.size(); ++i) {
        const Contact& contact = contacts_[i];
        if (math::Abs(contact.depth) > penetration) {
            penetration = math::Abs(contact.depth);
            //penetration = contact.depth;
            deepest = i;
        }
    }

    // find second contact
    size_t furthest1;
    float distanceSq1 = std::numeric_limits<float>::lowest();
    for (size_t i = 0; i < contacts_.size(); ++i) {
        if (i != deepest) {
            Contact contact = contacts_[i];
            float dist = (contact.pointA - contacts_[deepest].pointA).MagnitudeSq();
            if (dist > distanceSq1) {
                distanceSq1 = dist;
                furthest1 = i;
            }
        }
    }

    // find third contact
    size_t furthest2;
    float distanceSq2 = std::numeric_limits<float>::lowest();
    LineSegment ls(contacts_[deepest].pointA, contacts_[furthest1].pointA);
    for (size_t i = 0; i < contacts_.size(); ++i) {
        if (i != deepest && i != furthest1) {
            const Contact& contact = contacts_[i];
            float dist = ls.Distance(contact.pointA);
            if (dist > distanceSq2) {
                distanceSq2 = dist;
                furthest2 = i;
            }
        }
    }

    // find fourth contact
    size_t furthest3;
    float distanceSq3 = std::numeric_limits<float>::lowest();
    Triangle tri(contacts_[deepest].pointA, contacts_[furthest1].pointA, contacts_[furthest2].pointA);
    for (size_t i = 0; i < contacts_.size(); ++i) {
        if (i != deepest && i != furthest1 && i != furthest2) {
            const Contact& contact = contacts_[i];
            float dist = tri.ClosestPoint(contact.pointA).Distance(contact.pointA);
            if (dist > distanceSq3) {
                distanceSq3 = dist;
                furthest3 = i;
            }
        }
    }

    constexpr size_t total = 0 + 1 + 2 + 3 + 4;
    size_t drop = total - deepest - furthest1 - furthest2 - furthest3;
    contacts_.erase(contacts_.begin() + drop);

    if (furthest3 > drop) {
        --furthest3;
    }

    bool onTriangle = tri.Contains(contacts_[furthest3].pointA);
    if (onTriangle) {
        contacts_.erase(contacts_.begin() + furthest3);
    }
}

void ContactManifold::WarmStart(float ratio) {
    for (auto& contact : contacts_) {
        contact.WarmStart(ratio);
    }
}

void ContactManifold::Solve(float invDeltaTime) {
    for (auto& contact : contacts_) {
        contact.Solve(invDeltaTime);
    }
}

bool ContactManifold::IsOnIsland(uint32_t ver) {
    return island_ver_ == ver;
}

void ContactManifold::AddIsland(uint32_t ver) {
    island_ver_ = ver;
}

}
}

