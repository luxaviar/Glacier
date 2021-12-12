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
#include "render/base/vertexbuffer.h"
#include "render/base/indexbuffer.h"
#include "render/base/inputlayout.h"

namespace glacier {
namespace render {

const Color Gizmos::kDefaultColor = { 1.0f, 1.0f, 1.0f, 1.0f };

void Gizmos::Setup(GfxDriver* gfx) {
    vert_buffer_ = gfx->CreateVertexBuffer(nullptr, kMaxVertexCount * sizeof(GizmosVertex), 
        sizeof(GizmosVertex), UsageType::kDynamic);
    input_layout_ = gfx->CreateInputLayout(InputLayoutDesc{ InputLayoutDesc::Position3D, InputLayoutDesc::Texture2D });

    vert_data_.reserve(kMaxVertexCount * sizeof(GizmosVertex));
    Material wrire_mat("wireframe", TEXT("Gizmo"), TEXT("Gizmo"));

    RasterState rs;
    rs.depthWrite = false;
    //rs.fillMode = FillMode::kSolid;
    //rs.culling = CullingMode::kNone;

    rs.topology = TopologyType::kLine;
    wrire_mat.SetPipelineStateObject(gfx->CreatePipelineState(rs));
    materials_.push_back(wrire_mat);

    rs.topology = TopologyType::kTriangle;
    wrire_mat.SetPipelineStateObject(gfx->CreatePipelineState(rs));
    materials_.push_back(wrire_mat);

    rs.topology = TopologyType::kLineStrip;
    wrire_mat.SetPipelineStateObject(gfx->CreatePipelineState(rs));
    materials_.push_back(wrire_mat);

    rs.depthFunc = CompareFunc::kGreaterEqual;
    rs.blendEquationRGB = BlendEquation::kAdd;
    rs.blendFunctionSrcRGB = BlendFunction::kSrcAlpha;
    rs.blendFunctionDstRGB = BlendFunction::kOneMinusSrcAlpha;
    rs.blendEquationAlpha = BlendEquation::kAdd;
    rs.blendFunctionSrcAlpha = BlendFunction::kZero;
    rs.blendFunctionDstAlpha = BlendFunction::kDstAlpha;

    rs.topology = TopologyType::kLine;
    wrire_mat.SetPipelineStateObject(gfx->CreatePipelineState(rs));
    occluded_materials_.push_back(wrire_mat);

    rs.topology = TopologyType::kTriangle;
    wrire_mat.SetPipelineStateObject(gfx->CreatePipelineState(rs));
    occluded_materials_.push_back(wrire_mat);

    rs.topology = TopologyType::kLineStrip;
    wrire_mat.SetPipelineStateObject(gfx->CreatePipelineState(rs));
    occluded_materials_.push_back(wrire_mat);
}

Gizmos::Gizmos() :
    color_(kDefaultColor)
{
    
}

void Gizmos::Clear() {
    color_ = kDefaultColor;

    batchs_.clear();
    tasks_.clear();
    vert_data_.clear();
}

void Gizmos::SetColor(const Color& color) {
    color_ = LinearToGammaSpace(color);
    //color_ = color;
}

void Gizmos::DrawPrimitive(PrimitiveType type, RenderMode mode, GizmosVertex* vertex, uint32_t count) {
    if (!batchs_.empty()) {
        auto& b = batchs_.back();
        if (b.type == type && b.mode == mode && b.color == color_ && type != PrimitiveType::kLineStrip) {
            vert_data_.insert(vert_data_.end(), (uint8_t*)vertex, (uint8_t*)vertex + count * sizeof(GizmosVertex));
            b.vertex_count += count;
            return;
        }
    }

    GizmosBatch batch;
    batch.type = type;
    batch.mode = mode;
    batch.color = color_;
    batch.vertex_offset = (uint32_t)(vert_data_.size() / sizeof(GizmosVertex));
    batch.vertex_count = count;

    batchs_.push_back(batch);
    vert_data_.insert(vert_data_.end(), (uint8_t*)vertex, (uint8_t*)vertex + count * sizeof(GizmosVertex));
}

void Gizmos::Render(GfxDriver* gfx) {
    assert(vert_data_.size() <= kMaxVertexCount && "Too large gizmo vertex count");

    if (tasks_.empty()) return;
    EndLast();

    auto& view_mat = gfx->view();
    std::sort(tasks_.begin(), tasks_.end(), [&view_mat](const GizmosTask& a, const GizmosTask& b) {
        auto va = view_mat.MultiplyPoint3X4(a.center);
        auto vb = view_mat.MultiplyPoint3X4(b.center);
        return va.z < vb.z;
    });

    const auto& vp = gfx->projection() * view_mat;
    for (auto& mat : materials_) {
        mat.SetProperty("mvp_matrix", vp);
    }

    for (auto& mat : occluded_materials_) {
        mat.SetProperty("mvp_matrix", vp);
    }

    gfx->UpdateInputLayout(input_layout_);
    vert_buffer_->Update(vert_data_.data());
    vert_buffer_->Bind();

    BatchRender(gfx, materials_, 1.0f);
    BatchRender(gfx, occluded_materials_, 0.3f);
}

void Gizmos::BatchRender(GfxDriver* gfx, std::vector<Material>& mats, float alpha) {
    for (auto& task : tasks_) {
        for (auto i = task.begin; i < task.end; ++i) {
            auto& batch = batchs_[i];
            auto& mat = mats[(int)batch.type];
            batch.color.a = alpha;

            mat.SetProperty("line_color", batch.color);

            MaterialGuard guard(gfx, &mat);
            gfx->Draw(batch.vertex_count, batch.vertex_offset);
        }
    }
}

void Gizmos::Begin(const Vec3f& pos) {
    EndLast();

    GizmosTask task{
        pos,
        batchs_.size(),
        batchs_.size()
    };

    tasks_.push_back(task);
}

void Gizmos::EndLast() {
    if (tasks_.empty()) return;

    auto& t = tasks_.back();
    t.end = batchs_.size();
}

void Gizmos::DrawWireArc(const Vec3f& center, const Vec3f& normal, const Vec3f& from, float angle, 
    float radius, const Vec3f& translate, const Quaternion& rot)
{
    constexpr int kArcPoints = 60;
    Vec3f points[kArcPoints];
    geometry::CreateArcPoint(points, kArcPoints, center, normal, from, angle, radius);

    if (translate != Vec3f::zero || rot != Quaternion::identity) {
        for (auto& p : points) {
            p = rot * p + translate;
        }
    }

    //we don't use LineStrip, Line mode is batch friendly
    constexpr int kArcLinePoints = (kArcPoints - 1) * 2;
    GizmosVertex vertices[kArcLinePoints];
    for (int i = 0; i < kArcPoints - 1; ++i) {
        vertices[i * 2].pos = points[i];
        vertices[i * 2 + 1].pos = points[i + 1];
    }

    if (tasks_.empty()) {
        Begin(center);
    }

    DrawPrimitive(PrimitiveType::kLine, RenderMode::kWire, vertices, kArcLinePoints);
}

void Gizmos::DrawLine(const Vec3f& v0, const Vec3f& v1, const Vec3f& translate, const Quaternion& rot) {
    GizmosVertex vert[2];
    if (translate != Vec3f::zero || rot != Quaternion::identity) {
        vert[0].pos = rot * v0 + translate;
        vert[1].pos = rot * v1 + translate;
    } else {
        vert[0].pos = v0;
        vert[1].pos = v1;
    }

    if (tasks_.empty()) {
        Begin((vert[0].pos + vert[1].pos) / 2.0f);
    }

    DrawPrimitive(PrimitiveType::kLine, RenderMode::kWire, vert, 2);
}

void Gizmos::DrawCube(const Vec3f& center, const Vec3f extent, const Quaternion& rot) {
    Begin(center);

    Vec3f corner_points[] = {
        { -extent.x, -extent.y, -extent.z }, //p000 - 0
        { -extent.x, -extent.y, extent.z }, //p001 - 1
        { -extent.x, extent.y, -extent.z }, //p010 - 2
        { -extent.x, extent.y, extent.z }, //p011 - 3
        { extent.x, -extent.y, -extent.z }, //p100 - 4
        { extent.x, -extent.y, extent.z }, //p101 - 5
        { extent.x, extent.y, -extent.z }, //p110 - 6
        { extent.x, extent.y, extent.z } //p111 - 7
    };

    if (rot != Quaternion::identity) {
        for (auto& p : corner_points) {
            p = rot * p + center;
        }
    }
    else {
        for (auto& p : corner_points) {
            p = p + center;
        }
    }

    DrawLine(corner_points[0], corner_points[1]);
    DrawLine(corner_points[1], corner_points[3]);
    DrawLine(corner_points[3], corner_points[2]);
    DrawLine(corner_points[2], corner_points[0]);

    DrawLine(corner_points[4], corner_points[5]);
    DrawLine(corner_points[5], corner_points[7]);
    DrawLine(corner_points[7], corner_points[6]);
    DrawLine(corner_points[6], corner_points[4]);

    DrawLine(corner_points[0], corner_points[4]);
    DrawLine(corner_points[1], corner_points[5]);
    DrawLine(corner_points[2], corner_points[6]);
    DrawLine(corner_points[3], corner_points[7]);
}

void Gizmos::DrawSphere(const Vec3f& center, float radius) {
    Begin(center);
    DrawWireArc(center, Vec3f::up, Vec3f(1, 0, 0), 360.0f, radius);
    DrawWireArc(center, Vec3f::forward, Vec3f(1, 0, 0), 360.0f, radius);
    DrawWireArc(center, Vec3f::right, Vec3f(0, 1, 0), 360.0f, radius); 
}

void Gizmos::DrawCapsule(const Vec3f& center, float height, float radius, const Quaternion& rot) {
    Begin(center);

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
    Begin(center);

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
    Begin((corners[(int)FrustumCorner::kNearBottomLeft] + corners[(int)FrustumCorner::kFarTopRight]) / 2);

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

    auto center = mesh_renderer.world_bounds().Center();
    Begin(center);

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
