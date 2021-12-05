#pragma once

#include "Geometry/BezierCurve.h"

namespace glacier {

class GameObject;

namespace render {

class Material;

class BezierMesh {
public:
    static GameObject& CreatePlaneRoad(const BezierCurve& curve, float half_width,
        Material* material, bool flatten = true);

    static GameObject& CreateRoad(const BezierCurve& curve, float half_width, float thickness,
        Material* material, bool flatten = true);

    static GameObject& CreateTube(const BezierCurve& curve, float radius,
        int resolutionU, int resolutionV, Material* material);
};

}
}
