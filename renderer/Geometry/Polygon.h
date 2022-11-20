#pragma once

#include <vector>
#include "ray.h"
#include "Math/Util.h"
#include "LineSegment.h"
#include "Triangle2D.h"

namespace glacier {

struct Polygon {
    Polygon();
    Polygon(std::vector<Vector2>&& vert_list, const std::vector<int> tri_list);

    void Setup(std::vector<Vector2>&& vert_list, const std::vector<int> tri_list);

    Vector2 RandomPoint();
    bool CheckInnerDistance(const Vector2& inner_point, float margin);

    std::vector<Vector2> vertices;
    std::vector<LineSegment2D> segments;
    std::vector<Triangle2D> triangles;
    std::vector<float> weights;
    float total_weight;
};

}
