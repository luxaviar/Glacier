#pragma once

namespace glacier {

class Collider;

namespace physics {

struct RayHitResult {
    bool hit = false;
    float t;
    Collider* collider = nullptr;
};

}
}

