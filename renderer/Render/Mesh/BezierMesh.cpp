#include "BezierMesh.h"
#include "Math/Util.h"
#include "Core/GameObject.h"
#include "MeshRenderer.h"

namespace glacier {
namespace render {

GameObject& BezierMesh::CreatePlaneRoad(const BezierPath& curve, float half_width,
    const std::shared_ptr<Material>& material, bool flatten)
{
    VertexCollection vertices;
    IndexCollection indices;

    auto points = curve.GetPoints();
    int vert_size = points.size() * 2;
    size_t numTris = 2 * (points.size() - 1) + (curve.IsClosed() ? 2 : 0);

    vertices.reserve(vert_size);
    indices.reserve(numTris * 3);

    int vertIndex = 0;
    //int triIndex = 0;

    // Vertices for the top of the road are layed out:
    // 0  1
    // 2  3
    // and so on... So the triangle map 0,2,3 for example, defines a triangle from top left to bottom left to bottom right.
    constexpr int triangleMap[] = { 0, 2, 1, 1, 2, 3 };
    bool usePathNormals = !flatten;

    for (int i = 0; i < points.size(); i++)
    {
        auto& point = points[i];
        auto& tangent = point.tangent;
        auto& normal = point.normal;

        Vector3 localUp = (usePathNormals) ? tangent.Cross(normal) : curve.up();
        Vector3 localRight = (usePathNormals) ? normal : localUp.Cross(tangent);

        // Find position to left and right of current path vertex
        Vector3 vertSideA = point.position - localRight * math::Abs(half_width);
        Vector3 vertSideB = point.position + localRight * math::Abs(half_width);

        vertices.emplace_back(vertSideA, localUp, Vector2(0, point.time));
        vertices.emplace_back(vertSideB, localUp, Vector2(1, point.time));

        // Set triangle indices
        if (i < points.size() - 1 || curve.IsClosed())
        {
            for (int j = 0; j < std::size(triangleMap); j++)
            {
                indices.push_back((vertIndex + triangleMap[j]) % vert_size);
            }
        }

        vertIndex += 2;
    }

    auto mesh = std::make_shared<Mesh>(vertices, indices);

    auto& go = GameObject::Create("curve road");
    go.AddComponent<MeshRenderer>(mesh, material);

    return go;
}

GameObject& BezierMesh::CreateRoad(const BezierPath& curve, float half_width, float thickness,
    const std::shared_ptr<Material>& material, bool flatten)
{
    VertexCollection vertices;
    IndexCollection indices;

    auto points = curve.GetPoints();
    int vert_size = points.size() * 8;
    size_t numTris = 2 * (points.size() - 1) + (curve.IsClosed() ? 2 : 0);

    vertices.reserve(vert_size);
    indices.reserve(numTris * 3 * 4);

    int vertIndex = 0;
    //int triIndex = 0;

    // Vertices for the top of the road are layed out:
    // 0  1
    // 8  9
    // and so on... So the triangle map 0,8,1 for example, defines a triangle from top left to bottom left to bottom right.
    constexpr int triangleMap[] = { 0, 8, 1, 1, 8, 9 };
    constexpr int sidesTriangleMap[] = { 4, 6, 14, 12, 4, 14, 5, 15, 7, 13, 15, 5 };
    bool usePathNormals = !flatten;

    for (int i = 0; i < points.size(); i++)
    {
        auto& point = points[i];
        auto& tangent = point.tangent;
        auto& normal = point.normal;

        Vector3 localUp = (usePathNormals) ? tangent.Cross(normal) : curve.up();
        Vector3 localRight = (usePathNormals) ? normal : localUp.Cross(tangent);

        // Find position to left and right of current path vertex
        Vector3 vertSideA = point.position - localRight * math::Abs(half_width);
        Vector3 vertSideB = point.position + localRight * math::Abs(half_width);

        //top
        vertices.emplace_back(vertSideA, localUp, Vector2(0, point.time));
        vertices.emplace_back(vertSideB, localUp, Vector2(1, point.time));

        //bottom
        vertices.emplace_back(vertSideA - localUp * thickness, -localUp, Vector2{0, 0});
        vertices.emplace_back(vertSideB - localUp * thickness, -localUp, Vector2{ 0, 0 });

        //side
        vertices.emplace_back(vertSideA, -localRight, Vector2{ 0, 0 });
        vertices.emplace_back(vertSideB, localRight, Vector2{ 0, 0 });
        vertices.emplace_back(vertSideA - localUp * thickness, -localRight, Vector2{ 0, 0 });
        vertices.emplace_back(vertSideB - localUp * thickness, localRight, Vector2{ 0, 0 });

        // Set triangle indices
        if (i < points.size() - 1 || curve.IsClosed())
        {
            constexpr int tri_map_size = std::size(triangleMap);
            for (int j = 0; j < tri_map_size; j++)
            {
                indices.push_back((vertIndex + triangleMap[j]) % vert_size);
            }

            // reverse triangle map for under road so that triangles wind the other way and are visible from underneath
            for (int j = 0; j < tri_map_size; j++)
            {
                indices.push_back((vertIndex + triangleMap[tri_map_size - 1 - j] + 2) % vert_size);
            }

            for (int j = 0; j < std::size(sidesTriangleMap); j++) {
                indices.push_back((vertIndex + sidesTriangleMap[j]) % vert_size);
            }
        }

        vertIndex += 8;
    }

    auto mesh = std::make_shared<Mesh>(vertices, indices);

    auto& go = GameObject::Create("curve road");
    go.AddComponent<MeshRenderer>(mesh, material);

    return go;
}


// what is resolution U & V? let's unroll tube to rect
// | V: vertex count per unit length
// |
// |
// |
// |
// |
// |
// |----- U: vertex count for the circle
GameObject& BezierMesh::CreateTube(const BezierPath& curve, float radius, int resolutionU, int resolutionV, 
    const std::shared_ptr<Material>& material)
{
    VertexCollection vertices;
    IndexCollection indices;

    auto points = curve.GetPoints();

    int numCircles = math::Max(2, math::Round(curve.Length() * resolutionV) + 1);
    resolutionU = math::Clamp(resolutionU, 3, 30);

    for (int s = 0; s < numCircles; s++) {
        float segmentPercent = s / (numCircles - 1.0f);
        Vector3 centerPos = curve.GetPoint(segmentPercent);
        Vector3 norm = curve.GetNormal(segmentPercent);
        Vector3 forward = curve.GetTangent(segmentPercent);
        Vector3 tangentOrWhatEver = norm.Cross(forward);

        for (int currentRes = 0; currentRes < resolutionU; currentRes++) {
            auto angle = ((float)currentRes / resolutionU) * (math::kPI * 2.0f);

            auto xVal = math::Sin(angle) * radius;
            auto yVal = math::Cos(angle) * radius;

            auto point = (norm * xVal) + (tangentOrWhatEver * yVal) + centerPos;
            vertices.push_back(point);

            //! Adding the triangles
            if (s < numCircles - 1) {
                int startIndex = resolutionU * s;
                indices.push_back(startIndex + currentRes);
                indices.push_back(startIndex + (currentRes + 1) % resolutionU);
                indices.push_back(startIndex + currentRes + resolutionU);

                indices.push_back(startIndex + (currentRes + 1) % resolutionU);
                indices.push_back(startIndex + (currentRes + 1) % resolutionU + resolutionU);
                indices.push_back(startIndex + currentRes + resolutionU);
            }

        }
    }

    auto mesh = std::make_shared<Mesh>(vertices, indices, true);

    auto& go = GameObject::Create("curve tube");
    go.AddComponent<MeshRenderer>(mesh, material);

    return go;
}

}
}
