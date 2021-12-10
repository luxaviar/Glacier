#pragma once

#include <memory>
#include <unordered_map>
#include "render/material.h"
#include "Math/Mat4.h"
#include "geometry/frustum.h"
#include "Common/Singleton.h"
#include "render/base/gfxdriver.h"

namespace glacier {
namespace render {

class Camera;
class Mesh;
class MeshRenderer;

class Gizmos : public Singleton<Gizmos> {
public:
    enum class PrimitiveType : uint8_t {
        kLine = 0,
        kTriangle,
        kLineStrip,
        //kQuad
    };

    enum class RenderMode : uint8_t {
        kWire = 0,
        //kIcon
    };

    struct GizmosTask {
        Vec3f center;
        size_t begin;
        size_t end;
    };

    struct GizmosBatch {
        PrimitiveType type;
        RenderMode mode;
        Color color;
        uint32_t vertex_count;
        uint32_t vertex_offset;
    };

    struct GizmosVertex {
        Vec3f pos;
        //Colorf color;
        Vec2f coord;
    };

    static constexpr size_t kMaxVertexCount = 6553600;
    static const Color kDefaultColor;

    Gizmos();

    void Setup(GfxDriver* gfx);

    void SetColor(const Color& color);
    void Begin(const Vec3f& pos);

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

    //TODO: dynamic is too slow, let's put it on a standalone pass
    void DrawWireMesh(const MeshRenderer& mesh_renderer);

    void Render(GfxDriver* gfx);
    void Clear();

private:
    void BatchRender(GfxDriver* gfx, std::vector<Material>& psos, float alpha);
    void DrawPrimitive(PrimitiveType type, RenderMode mode, GizmosVertex* vertex, uint32_t count);
    void DrawWireMesh(const Mesh& mesh, const Matrix4x4& m);

    void EndLast();

    GfxDriver* gfx_;

    Color color_;
    std::shared_ptr<VertexBuffer> vert_buffer_;
    std::shared_ptr<InputLayout> input_layout_;

    //std::vector<RasterState> ps_;
    //std::vector<RasterState> occluded_ps_;
    //std::vector<Material> materials_; //mode -> material

    std::vector<Material> materials_;
    std::vector<Material> occluded_materials_;
    
    std::vector<uint8_t> vert_data_;
    std::vector<GizmosBatch> batchs_;
    std::vector<GizmosTask> tasks_;
};

}
}
