#pragma once

#include <stdint.h>
#include "common/freelist.h"
#include "Common/Uncopyable.h"
#include "contact.h"

namespace glacier {

class Collider;

namespace physics {

struct ContactPoint;

class ContactManifold : private Uncopyable {
public:
    friend struct Contact;

    ContactManifold(ContactManifold&& cm) noexcept;
    ContactManifold(Collider* a, Collider* b, uint64_t key, const ContactPoint& ci);
    ~ContactManifold();

    size_t count() const;
    uint64_t key() const;
    Collider* GetOther(Collider* me);
    void UpdateContact();
    void Add(const ContactPoint& ci);
    void Discard();
    void WarmStart(float ratio);
    void Solve(float invDeltaTime);
    bool IsOnIsland(uint32_t ver);
    void AddIsland(uint32_t ver);
    //void Clear();

    const std::vector<Contact>& GetContacts() const { return contacts_; }

private:
    Collider* a_;
    Collider* b_;

    float friction_;
    float restitution_;

    uint32_t island_ver_;
    uint64_t key_;

    std::vector<Contact> contacts_;
};

}
}
