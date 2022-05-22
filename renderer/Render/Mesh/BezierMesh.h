#pragma once

#include "Geometry/BezierPath.h"

namespace glacier {

class GameObject;

namespace render {

class Material;

class BezierMesh {
public:
    static GameObject& CreatePlaneRoad(const BezierPath& curve, float half_width,
        Material* material, bool flatten = true);

    static GameObject& CreateRoad(const BezierPath& curve, float half_width, float thickness,
        Material* material, bool flatten = true);

    static GameObject& CreateTube(const BezierPath& curve, float radius,
        int resolutionU, int resolutionV, Material* material);
};

}
}
