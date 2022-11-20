#pragma once

#include "Geometry/BezierPath.h"

namespace glacier {

class GameObject;

namespace render {

class Material;

class BezierMesh {
public:
    static GameObject& CreatePlaneRoad(const BezierPath& curve, float half_width,
        const std::shared_ptr<Material>& material, bool flatten = true);

    static GameObject& CreateRoad(const BezierPath& curve, float half_width, float thickness,
        const std::shared_ptr<Material>& material, bool flatten = true);

    static GameObject& CreateTube(const BezierPath& curve, float radius,
        int resolutionU, int resolutionV, const std::shared_ptr<Material>& material);
};

}
}
