#include "gizmos.h"
#include <memory>
#include <assert.h>
#include <algorithm>
#include "Math/Util.h"
#include "render/camera.h"
#include "Math/Vec3.h"
#include "Math/Mat4.h"
#include "common/color.h"
#include "Render/Mesh/geometry.h"
#include "Render/Mesh/MeshRenderer.h"
#include "render/base/buffer.h"
#include "render/base/inputlayout.h"
#include "Inspect/Profiler.h"

namespace glacier {
namespace render {

const Color Gizmos::kDefaultColor = { 1.0f, 1.0f, 1.0f, 1.0f };

Gizmos::Gizmos() :
    color_(kDefaultColor)
{

}

void Gizmos::Setup(GfxDriver* gfx) {
    auto vert_buffer = gfx->CreateVertexBuffer(nullptr, kMaxVertexCount * sizeof(GizmosVertex), 
        sizeof(GizmosVertex), UsageType::kDynamic);
    auto index_buffer = gfx->CreateIndexBuffer(nullptr, kMaxVertexCount * 2 * sizeof(uint32_t), IndexFormat::kUInt32, UsageType::kDynamic);

    mesh_ = std::make_unique<Mesh>(vert_buffer, index_buffer);

    auto input_layout = InputLayoutDesc{ InputLayoutDesc::Position3D, InputLayoutDesc::Float4Color };

    vert_data_.reserve(kMaxVertexCount * sizeof(GizmosVertex));
    index_data_.reserve(kMaxVertexCount * 2 * sizeof(uint32_t));
    material_ = std::make_shared<Material>("wireframe", TEXT("Gizmo"), TEXT("Gizmo"));

    RasterStateDesc rs;
    rs.depthWrite = false;
    rs.topology = TopologyType::kLine;
    material_->GetTemplate()->SetRasterState(rs);
    material_->GetTemplate()->SetInputLayout(input_layout);

    occluded_material_ = std::make_shared<Material>("wireframe", TEXT("Gizmo"), TEXT("Gizmo"));
    rs.depthFunc = RasterStateDesc::kReverseDepthFuncWithEqual;
    rs.blendEquationRGB = BlendEquation::kAdd;
    rs.blendFunctionSrcRGB = BlendFunction::kSrcAlpha;
    rs.blendFunctionDstRGB = BlendFunction::kOneMinusSrcAlpha;
    rs.blendEquationAlpha = BlendEquation::kAdd;
    rs.blendFunctionSrcAlpha = BlendFunction::kZero;
    rs.blendFunctionDstAlpha = BlendFunction::kDstAlpha;

    occluded_material_->GetTemplate()->SetRasterState(rs);
    occluded_material_->GetTemplate()->SetInputLayout(input_layout);
}

void Gizmos::OnDestroy() {
    mesh_.release();

    material_ = nullptr;
    occluded_material_ = nullptr;
}

void Gizmos::Clear() {
    color_ = kDefaultColor;

    vert_data_.clear();
    index_data_.clear();
}

void Gizmos::SetColor(const Color& color) {
    //color_ = LinearToGammaSpace(color);
    color_ = color;
}

void Gizmos::Draw(const GizmosVertex* vertex, uint32_t vert_count, const uint32_t* index, uint32_t index_count) {
    uint32_t index_offset = vert_data_.size();

    vert_data_.insert(vert_data_.end(), vertex, vertex + vert_count);
    for (uint32_t i = 0; i < index_count; ++i) {
        index_data_.push_back(index_offset + index[i]);
    }
}

void Gizmos::Render(GfxDriver* gfx, bool late) {
    if (vert_data_.empty()) return;

    assert(vert_data_.size() <= kMaxVertexCount && "Too large gizmo vertex count");

    auto& view_mat = gfx->view();

    const auto& vp = gfx->projection() * view_mat;
    material_->SetProperty("mvp_matrix", vp);
    occluded_material_->SetProperty("mvp_matrix", vp);

    {
        PerfSample("Update Buffer");
        mesh_->vertex_buffer()->Update(vert_data_.data(), vert_data_.size() * sizeof(GizmosVertex));
        mesh_->index_buffer()->Update(index_data_.data(), index_data_.size() * sizeof(uint32_t));
    }

    {
        PerfSample("Render");
        BatchRender(gfx, material_.get(), 1.0f);
    }

    {
        PerfSample("Occluded Render");
        BatchRender(gfx, occluded_material_.get(), 0.3f);
    }

    Clear();
}

void Gizmos::BatchRender(GfxDriver* gfx, Material* mat, float alpha) {
    mat->SetProperty("line_color", Color{0,0, 0, alpha});
    MaterialGuard guard(gfx, mat);
    mesh_->Draw(gfx);
}

void Gizmos::DrawWireArc(const Vec3f& center, const Vec3f& normal, const Vec3f& from, float angle, 
    float radius, const Vec3f& translate, const Quaternion& rot)
{
    constexpr int kArcPoints = 60;
    Vec3f points[kArcPoints];
    GizmosVertex vertices[kArcPoints];
    geometry::CreateArcPoint(points, kArcPoints, center, normal, from, angle, radius);

    if (translate != Vec3f::zero || rot != Quaternion::identity) {
        for (auto& p : points) {
            p = rot * p + translate;
        }
    }

    //we don't use LineStrip, Line mode is batch friendly
    constexpr int kArcLinePoints = (kArcPoints - 1) * 2;
    uint32_t indices[kArcLinePoints];
    for (int i = 0; i < kArcPoints - 1; ++i) {
        vertices[i] = { points[i], color_ };
        indices[i * 2] = i;
        indices[i * 2 + 1] = i + 1;
    }

    vertices[kArcPoints - 1] = { points[kArcPoints - 1], color_ };

    Draw(vertices, indices);
}

void Gizmos::DrawLine(const Vec3f& v0, const Vec3f& v1, const Vec3f& translate, const Quaternion& rot) {
    GizmosVertex vert[2];
    if (translate != Vec3f::zero || rot != Quaternion::identity) {
        vert[0] = { rot * v0 + translate, color_ };
        vert[1] = { rot * v1 + translate, color_ };
    } else {
        vert[0] = { v0, color_ };
        vert[1] = { v1, color_ };
    }

    Draw(vert, { 0, 1 });
}

void Gizmos::DrawCube(const Vec3f& center, const Vec3f extent, const Quaternion& rot) {
    GizmosVertex corner_points[] = {
        {{ -extent.x, -extent.y, -extent.z }, color_ }, //p000 - 0
        {{ -extent.x, -extent.y, extent.z }, color_ }, //p001 - 1
        {{ -extent.x, extent.y, -extent.z }, color_ }, //p010 - 2
        {{ -extent.x, extent.y, extent.z }, color_ }, //p011 - 3
        {{ extent.x, -extent.y, -extent.z }, color_ }, //p100 - 4
        {{ extent.x, -extent.y, extent.z }, color_ }, //p101 - 5
        {{ extent.x, extent.y, -extent.z }, color_ }, //p110 - 6
        {{ extent.x, extent.y, extent.z }, color_ } //p111 - 7
    };

    if (rot != Quaternion::identity) {
        for (auto& p : corner_points) {
            p.pos = rot * p.pos + center;
        }
    }
    else {
        for (auto& p : corner_points) {
            p.pos = p.pos + center;
        }
    }

    Draw(corner_points,
        {0, 1, 1, 3, 3, 2, 2, 0,
        4, 5, 5, 7, 7, 6, 6, 4,
        0, 4, 1, 5, 2, 6, 3, 7}
    );
}

void Gizmos::DrawSphere(const Vec3f& center, float radius) {
    DrawWireArc(center, Vec3f::up, Vec3f(1, 0, 0), 360.0f, radius);
    DrawWireArc(center, Vec3f::forward, Vec3f(1, 0, 0), 360.0f, radius);
    DrawWireArc(center, Vec3f::right, Vec3f(0, 1, 0), 360.0f, radius); 
}

void Gizmos::DrawCapsule(const Vec3f& center, float height, float radius, const Quaternion& rot) {
    float half_height = height * 0.5f;

    // lower cap
    DrawWireArc(Vec3f(0, half_height, 0), Vec3f(0, 1, 0), Vec3f(1, 0, 0), 360.0f, radius, center, rot);
    DrawWireArc(Vec3f(0, half_height, 0), Vec3f(1, 0, 0), Vec3f(0, 0, 1), -180.0f, radius, center, rot);
    DrawWireArc(Vec3f(0, half_height, 0), Vec3f(0, 0, 1), Vec3f(1, 0, 0), 180.0f, radius, center, rot);

    // upper cap
    DrawWireArc(Vec3f(0, -half_height, 0), Vec3f(0, 1, 0), Vec3f(1, 0, 0), 360.0f, radius, center, rot);
    DrawWireArc(Vec3f(0, -half_height, 0), Vec3f(1, 0, 0), Vec3f(0, 0, 1), 180.0f, radius, center, rot);
    DrawWireArc(Vec3f(0, -half_height, 0), Vec3f(0, 0, 1), Vec3f(1, 0, 0), -180.0f, radius, center, rot);

    // side lines
    DrawLine(Vec3f(+radius, half_height, 0.0f), Vec3f(+radius, -half_height, 0.0f), center, rot);
    DrawLine(Vec3f(-radius, half_height, 0.0f), Vec3f(-radius, -half_height, 0.0f), center, rot);
    DrawLine(Vec3f(0.0f, half_height, +radius), Vec3f(0.0f, -half_height, +radius), center, rot);
    DrawLine(Vec3f(0.0f, half_height, -radius), Vec3f(0.0f, -half_height, -radius), center, rot);
}

void Gizmos::DrawCylinder(const Vec3f& center, float height, float radius, const Quaternion& rot) {
    float half_height = height * 0.5f;

    // lower cap
    DrawWireArc(Vec3f(0, half_height, 0), Vec3f(0, 1, 0), Vec3f(1, 0, 0), 360.0f, radius, center, rot);

    // upper cap
    DrawWireArc(Vec3f(0, -half_height, 0), Vec3f(0, 1, 0), Vec3f(1, 0, 0), 360.0f, radius, center, rot);

    // side lines
    DrawLine(Vec3f(+radius, half_height, 0.0f), Vec3f(+radius, -half_height, 0.0f), center, rot);
    DrawLine(Vec3f(-radius, half_height, 0.0f), Vec3f(-radius, -half_height, 0.0f), center, rot);
    DrawLine(Vec3f(0.0f, half_height, +radius), Vec3f(0.0f, -half_height, +radius), center, rot);
    DrawLine(Vec3f(0.0f, half_height, -radius), Vec3f(0.0f, -half_height, -radius), center, rot);
}

void Gizmos::DrawFrustum(const Camera& camera) {
    Vec3f corners[(int)FrustumCorner::kCount];
    camera.FetchFrustumCorners(corners, camera.nearz(), camera.farz());

    DrawFrustum(corners);
}

void Gizmos::DrawFrustum(const Vec3f& position, float fov_degree, float aspect, float n, float f, const Quaternion& rot) {
    Vec3f corners[(int)FrustumCorner::kCount];
    geometry::FetchFrustumCorners(corners, CameraType::kPersp, fov_degree, aspect, n, f);

    if (position != Vec3f::zero || rot != Quaternion::identity) {
        for (auto& p : corners) {
            p = rot * p + position;
        }
    }

    DrawFrustum(corners);
}

void Gizmos::DrawFrustum(Vec3f corners[(int)FrustumCorner::kCount]) {
    DrawLine(corners[(int)FrustumCorner::kNearBottomLeft], corners[(int)FrustumCorner::kNearTopLeft]);
    DrawLine(corners[(int)FrustumCorner::kNearTopLeft], corners[(int)FrustumCorner::kNearTopRight]);
    DrawLine(corners[(int)FrustumCorner::kNearTopRight], corners[(int)FrustumCorner::kNearBottomRight]);
    DrawLine(corners[(int)FrustumCorner::kNearBottomRight], corners[(int)FrustumCorner::kNearBottomLeft]);

    DrawLine(corners[(int)FrustumCorner::kFarBottomLeft], corners[(int)FrustumCorner::kFarTopLeft]);
    DrawLine(corners[(int)FrustumCorner::kFarTopLeft], corners[(int)FrustumCorner::kFarTopRight]);
    DrawLine(corners[(int)FrustumCorner::kFarTopRight], corners[(int)FrustumCorner::kFarBottomRight]);
    DrawLine(corners[(int)FrustumCorner::kFarBottomRight], corners[(int)FrustumCorner::kFarBottomLeft]);

    DrawLine(corners[(int)FrustumCorner::kNearBottomLeft], corners[(int)FrustumCorner::kFarBottomLeft]);
    DrawLine(corners[(int)FrustumCorner::kNearTopLeft], corners[(int)FrustumCorner::kFarTopLeft]);
    DrawLine(corners[(int)FrustumCorner::kNearTopRight], corners[(int)FrustumCorner::kFarTopRight]);
    DrawLine(corners[(int)FrustumCorner::kNearBottomRight], corners[(int)FrustumCorner::kFarBottomRight]);
}

void Gizmos::DrawWireMesh(const MeshRenderer& mesh_renderer) {
    auto& meshes = mesh_renderer.meshes();
    auto& m = mesh_renderer.transform().LocalToWorldMatrix();

    //auto center = mesh_renderer.world_bounds().Center();
    
    for (auto& mesh : meshes) {
        DrawWireMesh(*mesh, m);
    }
}

void Gizmos::DrawWireMesh(const Mesh& mesh, const Matrix4x4& m) {
    auto& vertices = mesh.vertices();
    auto& indices = mesh.indices();

    std::vector<Vec3f> points;
    points.reserve(vertices.size());

    for (auto& v : vertices) {
        auto p = m.MultiplyPoint3X4(v.position);
        points.push_back(p);
    }

    for (size_t i = 0; i < indices.size(); i += 3) {
        auto& p0 = points[indices[i]];
        auto& p1 = points[indices[i + 1]];
        auto& p2 = points[indices[i + 2]];

        DrawLine(p0, p1);
        DrawLine(p1, p2);
        DrawLine(p2, p0);
    }
}

}
}
