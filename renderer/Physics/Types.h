#pragma once

#include <cstdint>

namespace glacier {

enum class RigidbodyType : int8_t {
    //dynamic: finite mass,  can only add force manually, collide with every one
    kDynamic = 0,
    //kinematic: infinite mass, velocity/position can be changed manually, do not collide with static or kinematic
    kKinematic = 1,
    //static: infinite mass, zero velocity,  position can be changed, do not collide with static or kinematic
    //kStatic = 2,
};

enum class RigidbodyLayer : uint32_t {
    kGround = 0,
    kPlayer = 1,
};

enum class RigidbodyTag : uint32_t {
    kTeamRed = 1 << 0,
    kTeamBlue = 1 << 1,
    kTeamAll = 3,
    kHero = 1 << 2,
    kSummon = 1 << 3,
    kUnit = (1 << 2) + (1 << 3),
    kBullet = 1 << 4,
};

}
