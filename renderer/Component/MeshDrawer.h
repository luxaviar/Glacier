#pragma once

#include <string>
#include "Math/Vec3.h"
#include "Core/Component.h"

namespace glacier {

class MeshDrawer : public Component
{
public:
    void OnDrawGizmos() override;
};

}
