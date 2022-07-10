#include "LightManager.h"
#include "Math/Util.h"
#include "Camera.h"
#include "Render/Material.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/Graph/PassNode.h"
#include "Render/Mesh/Geometry.h"
#include "Render/Mesh/MeshRenderer.h"
#include "Core/GameObject.h"
#include "Render/Renderer.h"
#include "Math/Vec4.h"
#include "Render/Base/Texture.h"
#include "Render/Base/RenderTarget.h"
#include "Render/Base/SamplerState.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/RenderTexturePool.h"
#include "Render/Base/Program.h"
#include "Render/Base/CommandQueue.h"

namespace glacier {
namespace render {

void LightManager::Setup(GfxDriver* gfx) {
    gfx_ = gfx;

    light_cbuffer_ = gfx->CreateConstantBuffer<LightList>();
    skybox_matrix_ = gfx_->CreateConstantBuffer<Matrix4x4>();

    GenerateSkybox();
    GenerateIrradiance();
    GenerateRadiance();
    GenerateBrdfLut();
}

void LightManager::Clear() {
    light_cbuffer_ = nullptr;

    skybox_material_ = nullptr;
    skybox_matrix_ = nullptr;

    skybox_texture_ = nullptr;
    radiance_ = nullptr;
    irradiance_ = nullptr;
    brdf_lut_ = nullptr;
}

void LightManager::Add(Light* light) {
    //assert(lights_.size() < NUM_LIGHTS);
    auto it = std::find(lights_.begin(), lights_.end(), light);
    assert(it == lights_.end());

    lights_.emplace_back(light);
}

void LightManager::Remove(Light* light) {
    auto it = std::find(lights_.begin(), lights_.end(), light);
    if (it != lights_.end()) {
        lights_.erase(it);
    }
}

void LightManager::SortLight(const Vec3f& pos) {
    std::sort(lights_.begin(), lights_.end(), 
        [pos] (Light* a, Light* b) {
            return a->LuminessAtPoint(pos) > b->LuminessAtPoint(pos);
        });
}

DirectionalLight* LightManager::GetMainLight() {
    if (lights_.empty()) return nullptr;

    auto first = lights_[0];
    if (first->type() == LightType::kDirectinal) {
        return (DirectionalLight*)first;
    }

    return nullptr;
}

void LightManager::SetupMaterial(Material* mat) {
    if (mat->HasParameter("LightList")) {
        mat->SetProperty("LightList", light_cbuffer_);
    }
    if (mat->HasParameter("_BrdfLutTexture")) {
        mat->SetProperty("_BrdfLutTexture", brdf_lut_);
    }
    if (mat->HasParameter("_RadianceTextureCube")) {
        mat->SetProperty("_RadianceTextureCube", radiance_);
    }
    if (mat->HasParameter("_IrradianceTextureCube")) {
        mat->SetProperty("_IrradianceTextureCube", irradiance_);
    }
}

void LightManager::Update(CommandBuffer* cmd_buffer) {
    size_t i = 0;
    for (; i < lights_.size() && i < kMaxLightNum; ++i) {
        auto light = lights_[i];
        light->Update(cmd_buffer);
        auto sz = sizeof(light->data_);
        memcpy(&light_data_.lights[i], &light->data_, sz);
    }

    for (; i < kMaxLightNum; ++i) {
        light_data_.lights[i].enable = false;
    }

    light_cbuffer_->Update(&light_data_);
}

void LightManager::AddSkyboxPass(Renderer* renderer) {
    renderer->render_graph().AddPass("skybox",
        [&](PassNode& pass) {
        },
        [=](CommandBuffer* cmd_buffer, const PassNode& pass) {
            auto main_camera_ = renderer->GetMainCamera();
            if (main_camera_->type() != CameraType::kPersp) return;

            renderer->BindLightingTarget(cmd_buffer);

            auto trans = cmd_buffer->projection() * cmd_buffer->view();
            skybox_matrix_->Update(&trans);

            pass.Render(cmd_buffer, skybox_cube_, skybox_material_.get());
        });
}

void LightManager::GenerateSkybox() {
    auto cmd_buffer = gfx_->GetCommandBuffer(CommandBufferType::kDirect);

    skybox_texture_ = cmd_buffer->CreateTextureFromFile(TEXT("assets\\images\\valley_skybox.dds"), false, false, TextureType::kTextureCube);
    skybox_material_ = std::make_unique<Material>("skybox", TEXT("Skybox"), TEXT("Skybox"));

    RasterStateDesc rs;
    rs.depthWrite = false;
    rs.depthFunc = RasterStateDesc::kDefaultDepthFuncWithEqual;
    rs.culling = CullingMode::kFront;
    skybox_material_->GetProgram()->SetRasterState(rs);
    skybox_material_->GetProgram()->SetInputLayout(InputLayoutDesc{ InputLayoutDesc::Position3D });

    skybox_material_->SetProperty("vp_matrix", skybox_matrix_);
    skybox_material_->SetProperty("_SkyboxTextureCube", skybox_texture_);
    skybox_material_->SetProperty("linear_sampler", SamplerState{});

    VertexCollection vertices;
    IndexCollection indices;
    geometry::CreateCube(vertices, indices, { 1, 1, 1 });
    auto box_mesh = std::make_shared<Mesh>(vertices, indices);
    auto& skybox_go = GameObject::Create("skybox");
    skybox_go.DontDestroyOnLoad(true);
    skybox_go.Hide();

    skybox_cube_ = skybox_go.AddComponent<MeshRenderer>(box_mesh);
    skybox_cube_->SetCastShadow(false);
    skybox_cube_->SetReciveShadow(false);

    auto cmd_queue = gfx_->GetCommandQueue(CommandBufferType::kDirect);
    cmd_queue->ExecuteCommandBuffer(cmd_buffer);
    cmd_queue->Flush();
}

void LightManager::GenerateBrdfLut() {
    auto cmd_buffer = gfx_->GetCommandBuffer(CommandBufferType::kDirect);

    RasterStateDesc rs;
    rs.depthWrite = false;
    rs.depthEnable = false;
    rs.depthFunc = CompareFunc::kAlways;

    auto integrate_material_ = std::make_unique<PostProcessMaterial>("integrate", TEXT("IntegrateBRDF"));
    integrate_material_->GetProgram()->SetRasterState(rs);

    constexpr int kSize = 512;
    brdf_lut_ = RenderTexturePool::Get(kSize, kSize, TextureFormat::kR16G16B16A16_FLOAT);
    brdf_lut_->SetName("BRDF LUT texture");

    auto lut_target = gfx_->CreateRenderTarget(kSize, kSize);

    lut_target->AttachColor(AttachmentPoint::kColor0, brdf_lut_);

    Renderer::PostProcess(cmd_buffer, lut_target, integrate_material_.get());

    auto cmd_queue = gfx_->GetCommandQueue(CommandBufferType::kDirect);
    cmd_queue->ExecuteCommandBuffer(cmd_buffer);
    cmd_queue->Flush();
}

void LightManager::GenerateIrradiance() {
    auto cmd_buffer = gfx_->GetCommandBuffer(CommandBufferType::kDirect);

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kClamp;

    auto convolve_material_ = std::make_unique<Material>("convolve", TEXT("EnvIrradiance"), TEXT("EnvIrradiance"));
    auto convolve_matrix_ = gfx_->CreateConstantBuffer<Matrix4x4>();

    convolve_material_->SetProperty("linear_sampler", ss);
    convolve_material_->SetProperty("_SkyboxTextureCube", skybox_texture_);
    convolve_material_->SetProperty("vp_matrix", convolve_matrix_);

    RasterStateDesc rs;
    rs.depthWrite = false;
    rs.depthEnable = false;
    rs.depthFunc = CompareFunc::kAlways;
    rs.culling = CullingMode::kFront;
    convolve_material_->GetProgram()->SetRasterState(rs);
    convolve_material_->GetProgram()->SetInputLayout(InputLayoutDesc{ InputLayoutDesc::Position3D });

    auto constexpr count = toUType(CubeFace::kCount);
    Matrix4x4 view[count] = {
        Matrix4x4::LookToLH(Vec3f::zero, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }), //+x
        Matrix4x4::LookToLH(Vec3f::zero, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}), //-x
        Matrix4x4::LookToLH(Vec3f::zero, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}), //+y
        Matrix4x4::LookToLH(Vec3f::zero, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}), //-y
        Matrix4x4::LookToLH(Vec3f::zero, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}), //+z
        Matrix4x4::LookToLH(Vec3f::zero, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}) //-z
    };

    Matrix4x4 project = Matrix4x4::PerspectiveFovLH(90.0f * math::kDeg2Rad, 1.0f, 0.1f, 10.0f);

    constexpr int kSize = 256;
    auto desc = Texture::Description()
        .SetType(TextureType::kTextureCube)
        .SetFormat(TextureFormat::kR16G16B16A16_FLOAT)
        .SetCreateFlag(CreateFlags::kRenderTarget)
        .SetDimension(kSize, kSize)
        .EnableMips();

    irradiance_ = gfx_->CreateTexture(desc);
    irradiance_->SetName("Irradiance texture");

    auto irrandiance_target = gfx_->CreateRenderTarget(kSize, kSize);

    for (int i = 0; i < count; ++i) {
        Matrix4x4 vp = project * view[i];
        convolve_matrix_->Update(&vp);
        irrandiance_target->AttachColor(AttachmentPoint::kColor0, irradiance_, i);

        RenderTargetGuard rt_guard(cmd_buffer, irrandiance_target.get());
        skybox_cube_->Render(cmd_buffer, convolve_material_.get());
    }

    cmd_buffer->GenerateMipMaps(irradiance_.get());

    auto cmd_queue = gfx_->GetCommandQueue(CommandBufferType::kDirect);
    cmd_queue->ExecuteCommandBuffer(cmd_buffer);
    cmd_queue->Flush();
}

void LightManager::GenerateRadiance() {
    auto cmd_buffer = gfx_->GetCommandBuffer(CommandBufferType::kDirect);

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kClamp;

    auto prefiter_matrix = gfx_->CreateConstantBuffer<Matrix4x4>();
    auto roughness = gfx_->CreateConstantBuffer<Vec4f>();

    auto prefilter_material = std::make_unique<Material>("convolve", TEXT("EnvRadiance"), TEXT("EnvRadiance"));

    RasterStateDesc rs;
    rs.depthWrite = false;
    rs.depthEnable = false;
    rs.depthFunc = CompareFunc::kAlways;
    rs.culling = CullingMode::kFront;
    prefilter_material->GetProgram()->SetRasterState(rs);
    prefilter_material->GetProgram()->SetInputLayout(InputLayoutDesc{ InputLayoutDesc::Position3D });

    prefilter_material->SetProperty("linear_sampler", ss);
    prefilter_material->SetProperty("_SkyboxTextureCube", skybox_texture_);
    prefilter_material->SetProperty("vp_matrix", prefiter_matrix);
    prefilter_material->SetProperty("Roughness", roughness);

    auto constexpr count = toUType(CubeFace::kCount);
    Matrix4x4 view[count] = {
        Matrix4x4::LookToLH(Vec3f::zero, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }), //+x
        Matrix4x4::LookToLH(Vec3f::zero, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}), //-x
        Matrix4x4::LookToLH(Vec3f::zero, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}), //+y
        Matrix4x4::LookToLH(Vec3f::zero, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}), //-y
        Matrix4x4::LookToLH(Vec3f::zero, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}), //+z
        Matrix4x4::LookToLH(Vec3f::zero, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}) //-z
    };

    Matrix4x4 project = Matrix4x4::PerspectiveFovLH(90.0f * math::kDeg2Rad, 1.0f, 0.1f, 10.0f);

    constexpr int kSize = 512;
    auto desc = Texture::Description()
        .SetType(TextureType::kTextureCube)
        .SetDimension(kSize, kSize)
        .SetFormat(TextureFormat::kR16G16B16A16_FLOAT)
        .SetCreateFlag(CreateFlags::kRenderTarget)
        .EnableMips();
    radiance_ = gfx_->CreateTexture(desc);
    radiance_->SetName("Radiance texture");

    uint32_t mip_levels = radiance_->GetMipLevels();

    light_data_.radiance_max_load = (float)mip_levels - 1;

    for (uint32_t j = 0; j < mip_levels; ++j) {
        int size = kSize >> j;
        auto randiance_target = gfx_->CreateRenderTarget(size, size);
        for (int i = 0; i < count; ++i) {
            Matrix4x4 vp = project * view[i];
            prefiter_matrix->Update(&vp);

            Vec4f roughness_for_mip;
            roughness_for_mip.x = (float)j / (mip_levels - 1);
            roughness->Update(&roughness_for_mip);
            randiance_target->AttachColor(AttachmentPoint::kColor0, radiance_, i , j);

            RenderTargetGuard rt_guard(cmd_buffer, randiance_target.get());
            skybox_cube_->Render(cmd_buffer, prefilter_material.get());
        }
    }

    auto cmd_queue = gfx_->GetCommandQueue(CommandBufferType::kDirect);
    cmd_queue->ExecuteCommandBuffer(cmd_buffer);
    cmd_queue->Flush();
}

}
}
