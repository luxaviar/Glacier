#pragma once

#include <memory>
#include <unordered_map>
#include "render/material.h"
#include "Math/Mat4.h"
#include "geometry/frustum.h"
#include "Common/Singleton.h"
#include "render/base/gfxdriver.h"
#include "Render/Mesh/Mesh.h"

namespace glacier {
namespace render {

class Camera;
class Mesh;
class MeshRenderer;
class CommandBuffer;

class Gizmos : public Singleton<Gizmos> {
public:
    //enum class PrimitiveType : uint8_t {
    //    kLine = 0,
    //    kTriangle,
    //    kLineStrip,
    //    //kQuad
    //};

    //enum class RenderMode : uint8_t {
    //    kWire = 0,
    //    //kIcon
    //};

    struct GizmosVertex {
        Vec3f pos;
        Color color;
        //Vec2f coord;
    };

    static constexpr size_t kMaxVertexCount = 655360;
    static const Color kDefaultColor;

    Gizmos();

    void Setup(GfxDriver* gfx);
    void OnDestroy();

    void SetColor(const Color& color);

    void DrawLine(const Vec3f& v0, const Vec3f& v1, 
        const Vec3f& translate = Vec3f::zero, const Quaternion& rot = Quaternion::identity);

    void DrawWireArc(const Vec3f& center, const Vec3f& normal, const Vec3f& from, float angle, float radius,
        const Vec3f& translate = Vec3f::zero, const Quaternion& rot = Quaternion::identity);

    void DrawSphere(const Vec3f& center, float radius);
    void DrawCube(const Vec3f& center, const Vec3f extent, const Quaternion& rot = Quaternion::identity);
    void DrawCapsule(const Vec3f& center, float height, float radius, const Quaternion& rot = Quaternion::identity);
    void DrawCylinder(const Vec3f& center, float height, float radius, const Quaternion& rot = Quaternion::identity);

    void DrawFrustum(const Camera& camera);
    void DrawFrustum(const Vec3f& position, float fov_degree, float aspect, float n, float f, const Quaternion& rot = Quaternion::identity);
    void DrawFrustum(Vec3f corners[(int)FrustumCorner::kCount]);

    void DrawWireMesh(const MeshRenderer& mesh_renderer);

    void Render(CommandBuffer* cmd_buffer, bool late = false);
    void Clear();

private:
    void BatchRender(CommandBuffer* cmd_buffer, Material* mat, float alpha);

    void Draw(const GizmosVertex* vertex, uint32_t vert_count,
        const uint32_t* index, uint32_t index_count);
    void DrawWireMesh(const Mesh& mesh, const Matrix4x4& m);

    template<size_t N, size_t M>
    void Draw(const GizmosVertex (&vertices)[N], const uint32_t (&indices)[M]) {
        Draw(vertices, N, indices, M);
    }

    GfxDriver* gfx_;

    Color color_;
    std::unique_ptr<Mesh> mesh_;

    std::shared_ptr<Material> material_;
    std::shared_ptr<Material> occluded_material_;

    std::vector<GizmosVertex> vert_data_;
    std::vector<uint32_t> index_data_;
};

}
}
