#include "LayerCollisionFilter.h"
#include <assert.h>
#include "Physics/Collider/Collider.h"

namespace glacier {
namespace physics {

LayerCollisionFilter::LayerCollisionFilter() {
    //default state: layers collide with each other except self
    for (int i = 0; i < kSize; ++i) {
        collision_matrix_[i] = ~(1 << i);//unchecked((int)0xFFFFFFFF);
    }
}

void LayerCollisionFilter::SetCollision(int layer1, int layer2, bool ignore) {
    assert(layer1 >= 0 && layer1 < kSize);
    assert(layer2 >= 0 && layer2 < kSize);

    if (ignore) {
        collision_matrix_[layer1] &= (~(1 << layer2));
    } else {
        collision_matrix_[layer1] |= (1 << layer2);
    }
}

bool LayerCollisionFilter::CanCollide(Collider* a, Collider* b) const {
    return (collision_matrix_[a->layer()] & (1 << b->layer())) != 0;
}

bool LayerCollisionFilter::CanCollide(Collider* a) const {
    return true;
}

}
}

