#pragma once

#include <stdint.h>
#include "Physics/CollisionFilter.h"

namespace glacier {
namespace physics {

//Each layer can collide with other layers by default, but not including self
class LayerCollisionFilter : public CollisionFilter {
public:
    constexpr static int kSize = sizeof(uint32_t) * 8;
    LayerCollisionFilter();
    void SetCollision(int layer1, int layer2, bool ignore);
    bool CanCollide(Collider* a, Collider* b) const override;
    bool CanCollide(Collider* a) const override;

private:
    uint32_t collision_matrix_[kSize];
};

}
}
