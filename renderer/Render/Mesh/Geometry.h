#pragma once

#include <vector>
#include "Math/Vec3.h"
#include "Math/Vec2.h"
#include "render/base/enums.h"
#include "geometry/frustum.h"

namespace glacier {
namespace render {

struct CommonVertex {
    CommonVertex(const Vec3f& iposition, const Vec3f& inormal = { 0, 0, 1 }, const Vec2f& itexcoord = Vec2f::zero,
        const Vec3f& tangent = Vec3f::zero, const Vec3f& bitangent = Vec3f::zero) noexcept :
        position(iposition),
        normal(inormal),
        texcoord(itexcoord),
        tangent(tangent),
        bitangent(bitangent)
    {
    }

    Vec3f position;
    Vec3f normal;
    Vec2f texcoord;
    Vec3f tangent;
    Vec3f bitangent;
};

using VertexCollection = std::vector<CommonVertex>;
using IndexCollection = std::vector<uint32_t>;

class Camera;

namespace geometry {

void CalcTangent(VertexCollection& vertices, IndexCollection& indices);

void CreateCube(VertexCollection& vertices, IndexCollection& indices, 
    const Vec3f& size = { 1.0f, 1.0f, 1.0f }, bool rhcoords = false, bool invertn = false);

void CreateSphere(VertexCollection& vertices, IndexCollection& indices,
    float radius, uint16_t tessellation = 12, bool rhcoords =false, bool invertn = false);

void CreateCylinder(VertexCollection& vertices, IndexCollection& indices,
    float height, float radius, uint16_t tessellation = 12, bool rhcoords = false, bool invertn = false);

void CreateCapsule(VertexCollection& vertices, IndexCollection& indices,
    float height, float radius, uint16_t tessellation = 12, bool rhcoords = false, bool invertn = false);

void CreateTetrahedron(VertexCollection& vertices, IndexCollection& indices, float size, bool rhcoords);

void CreateFrustumIndicator(VertexCollection& vertices, IndexCollection& indices);

void CreateFrustum(VertexCollection& vertices, IndexCollection& indices,
    float width, float height, float nearZ, float farZ);

void CreateFrustum(VertexCollection& vertices, IndexCollection& indices, const Camera& camera);

void CreateQuad(VertexCollection& vertices, IndexCollection& indices);

void CreateArcPoint(Vec3f* vertices, int count, const Vec3f& center, const Vec3f& normal,
    const Vec3f& from, float angle, float radius);

void FetchFrustumCorners(Vec3f corners[(int)FrustumCorner::kCount], CameraType type, 
    float fov_degree_or_width, float aspect_or_height, float n, float f);

void FetchFrustumPlanes(Plane planes[(int)FrustumPlane::kCount], const Vec3f corners[(int)FrustumCorner::kCount]);

}
}
}
