#pragma once

#include <string>
#include "Math/Vec3.h"
#include "core/behaviour.h"

namespace glacier {

class Fall : public Behaviour
{
public:
    void Update(float dt) override;

private:
    void SpawnCube();
    void SpawnSphere();
    void SpawnCylinder();
    void SpawnCapsule();
};

}
