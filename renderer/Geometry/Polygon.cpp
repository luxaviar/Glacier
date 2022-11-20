#include <limits>
#include <math.h>
#include "Polygon.h"
#include "Math/Random.h"

namespace glacier {

Polygon::Polygon() {

}

Polygon::Polygon(std::vector<Vector2>&& vert_list, const std::vector<int> tri_list)
{
    Setup(std::move(vert_list), tri_list);
}

void Polygon::Setup(std::vector<Vector2>&& vert_list, const std::vector<int> tri_list) {
    assert(vert_list.size() >= 3);

    segments.clear();
    triangles.clear();
    weights.clear();
    total_weight = 0;
    std::swap(vertices, vert_list);

    for (size_t i = 1; i < vertices.size(); ++i) {
        auto p0 = vertices[i - 1];
        auto p1 = vertices[i];
        segments.push_back({ p0, p1 });
    }
    segments.push_back({ vertices.back(), vertices.front() });

    for (size_t i = 0; i < tri_list.size(); i+=3) {
        auto a = vertices[tri_list[i]];
        auto b = vertices[tri_list[i + 1]];
        auto c = vertices[tri_list[i + 2]];

        Triangle2D tri{ a, b, c };
        triangles.push_back(tri);

        float weight = tri.Area();
        total_weight += weight;

        weights.push_back(weight);
    }
}

Vector2 Polygon::RandomPoint() {
    float random_weight = random::Range(0.0f, total_weight);
    size_t idx = triangles.size() - 1;
    for (size_t i = 0; i < triangles.size(); ++i) {
        if (weights[i] >= random_weight) {
            idx = i;
            break;
        }
        else {
            random_weight -= weights[i];
        }
    }

    auto& tri = triangles[idx];
    auto point = tri.RandomPointInside();

    return point;
}

bool Polygon::CheckInnerDistance(const Vector2& inner_point, float margin) {
    float margin_sq = margin * margin;
    for (auto& seg : segments) {
        if (seg.DistanceSq(inner_point) < margin_sq) {
            return false;
        }
    }

    return true;
}

}
