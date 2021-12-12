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
#include "Render/Base/ConstantBuffer.h"
#include "Render/Base/Sampler.h"

namespace glacier {
namespace render {

void LightManager::Setup(GfxDriver* gfx) {
    gfx_ = gfx;

    light_cbuffer_ = gfx->CreateConstantBuffer<LightList>();

    GenerateSkybox();
    GenerateIrradiance();
    GenerateRadiance();
    GenerateBrdfLut();
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

void LightManager::Bind() {
    BindLightList();
    BindEnv();
}

void LightManager::UnBind() {
    light_cbuffer_->UnBind(ShaderType::kPixel, 1);
    UnBindEnv();
}

void LightManager::BindLightList() {
    size_t i = 0;
    for (; i < lights_.size() && i < kMaxLightNum; ++i) {
        auto light = lights_[i];
        light->Update(gfx_);
        auto sz = sizeof(light->data_);
        memcpy(&light_data_.lights[i], &light->data_, sz);
    }

    for (; i < kMaxLightNum; ++i) {
        light_data_.lights[i].enable = false;
    }

    light_cbuffer_->Update(&light_data_);
    light_cbuffer_->Bind(ShaderType::kPixel, 1);
}

void LightManager::BindEnv() {
    if (radiance_) {
        radiance_->Bind(ShaderType::kPixel, 2);
    }

    if (irradiance_) {
        irradiance_->Bind(ShaderType::kPixel, 3);
    }

    if (brdf_lut_) {
        brdf_lut_->Bind(ShaderType::kPixel, 1);
    }
}

void LightManager::UnBindEnv() {
    if (radiance_) {
        radiance_->UnBind(ShaderType::kPixel, 2);
    }

    if (irradiance_) {
        irradiance_->UnBind(ShaderType::kPixel, 3);
    }

    if (brdf_lut_) {
        brdf_lut_->UnBind(ShaderType::kPixel, 1);
    }
}

void LightManager::AddSkyboxPass(Renderer* renderer) {
    renderer->render_graph().AddPass("skybox",
        [&](PassNode& pass) {
        },
        [=](Renderer* renderer, const PassNode& pass) {
            auto main_camera_ = renderer->GetMainCamera();
            if (main_camera_->type() != CameraType::kPersp) return;

            auto gfx = renderer->driver();
            gfx->BindCamera(main_camera_);

            auto trans = gfx->projection() * gfx->view();
            skybox_matrix_->Update(&trans);

            pass.Render(renderer, skybox_cube_, skybox_material_.get());
        });
}

void LightManager::GenerateSkybox() {
    skybox_matrix_ = gfx_->CreateConstantBuffer<Matrix4x4>();// std::make_shared<ConstantBuffer<Matrix4x4>>(gfx);

    auto builder = Texture::Builder()
        .SetFile(TEXT("assets\\images\\valley_skybox.dds"))
        .SetType(TextureType::kTextureCube);
    skybox_texture_ = gfx_->CreateTexture(builder);

    auto skybox_sampler = gfx_->CreateSampler(SamplerState{});
    skybox_material_ = std::make_unique<Material>("skybox", TEXT("Skybox"), TEXT("Skybox"));

    RasterState rs;
    rs.depthWrite = false;
    rs.depthFunc = CompareFunc::kLessEqual;
    rs.culling = CullingMode::kFront;
    skybox_material_->SetPipelineStateObject(rs);

    skybox_material_->SetProperty("vp_matrix", skybox_matrix_);
    skybox_material_->SetProperty("tex", skybox_texture_);
    skybox_material_->SetProperty("tex_sam", skybox_sampler);

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
}

void LightManager::GenerateBrdfLut() {
    RasterState rs;
    rs.depthWrite = false;
    rs.depthEnable = false;
    rs.depthFunc = CompareFunc::kAlways;

    auto integrate_material_ = std::make_unique<Material>("integrate", TEXT("IntegrateBRDF"), TEXT("IntegrateBRDF"));
    integrate_material_->SetPipelineStateObject(rs);

    constexpr int kSize = 512;
    auto builder = Texture::Builder()
        .SetFormat(TextureFormat::kR16G16B16A16_FLOAT)
        .SetDimension(kSize, kSize);
    brdf_lut_ = gfx_->CreateTexture(builder);
    auto lut_target = gfx_->CreateRenderTarget(kSize, kSize);

    lut_target->AttachColor(AttachmentPoint::kColor0, brdf_lut_);

    VertexCollection vertices;
    IndexCollection indices;
    geometry::CreateQuad(vertices, indices);
    auto fullscreen_quad_mesh = std::make_shared<Mesh>(vertices, indices);

    auto& quad_go = GameObject::Create("fullscreen quad");
    quad_go.DontDestroyOnLoad(true);
    quad_go.Hide();

    auto quad = quad_go.AddComponent<MeshRenderer>(fullscreen_quad_mesh);
    quad->SetPickable(false);
    quad->SetCastShadow(false);
    quad->SetReciveShadow(false);

    MaterialGuard mat_guard(gfx_, integrate_material_.get());
    RenderTargetGuard rt_guard(lut_target.get());

    quad->Render(gfx_);
}

void LightManager::GenerateIrradiance() {
    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kClamp;

    auto convolve_material_ = std::make_unique<Material>("convolve", TEXT("EnvIrradiance"), TEXT("EnvIrradiance"));
    auto linear_sampler = gfx_->CreateSampler(ss);
    auto convolve_matrix_ = gfx_->CreateConstantBuffer<Matrix4x4>();

    convolve_material_->SetProperty("tex_sam", linear_sampler);
    convolve_material_->SetProperty("tex", skybox_texture_);
    convolve_material_->SetProperty("vp_matrix", convolve_matrix_);

    RasterState rs;
    rs.depthWrite = false;
    rs.depthEnable = false;
    rs.depthFunc = CompareFunc::kAlways;
    rs.culling = CullingMode::kFront;
    convolve_material_->SetPipelineStateObject(rs);

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
    auto builder = Texture::Builder()
        .SetType(TextureType::kTextureCube)
        .SetFormat(TextureFormat::kR16G16B16A16_FLOAT)
        .SetDimension(kSize, kSize)
        .EnableMips();

    irradiance_ = gfx_->CreateTexture(builder);
    auto irrandiance_target = gfx_->CreateRenderTarget(kSize, kSize);

    for (int i = 0; i < count; ++i) {
        Matrix4x4 vp = project * view[i];
        convolve_matrix_->Update(&vp);
        irrandiance_target->AttachColor(AttachmentPoint::kColor0, irradiance_, i);

        MaterialGuard mat_guard(gfx_, convolve_material_.get());
        RenderTargetGuard rt_guard(irrandiance_target.get());

        skybox_cube_->Render(gfx_);
    }

    irradiance_->GenerateMipMaps();
}

void LightManager::GenerateRadiance() {
    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kClamp;
    auto linear_sampler = gfx_->CreateSampler(ss);
    auto prefiter_matrix_ = gfx_->CreateConstantBuffer<Matrix4x4>();
    auto roughness = gfx_->CreateConstantBuffer<Vec4f>();

    auto prefilter_material_ = std::make_unique<Material>("convolve", TEXT("EnvRadiance"), TEXT("EnvRadiance"));

    RasterState rs;
    rs.depthWrite = false;
    rs.depthEnable = false;
    rs.depthFunc = CompareFunc::kAlways;
    rs.culling = CullingMode::kFront;
    prefilter_material_->SetPipelineStateObject(rs);

    prefilter_material_->SetProperty("tex_sam", linear_sampler);
    prefilter_material_->SetProperty("tex", skybox_texture_);
    prefilter_material_->SetProperty("vp_matrix", prefiter_matrix_);
    prefilter_material_->SetProperty("Roughness", roughness);

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
    auto builder = Texture::Builder()
        .SetType(TextureType::kTextureCube)
        .SetDimension(kSize, kSize)
        .SetFormat(TextureFormat::kR16G16B16A16_FLOAT)
        .EnableMips();
    radiance_ = gfx_->CreateTexture(builder);
    auto randiance_target = gfx_->CreateRenderTarget(kSize, kSize);
    uint32_t mip_levels = radiance_->GetMipLevels();

    light_data_.radiance_max_load = (float)mip_levels - 1;

    for (uint32_t j = 0; j < mip_levels; ++j) {
        for (int i = 0; i < count; ++i) {
            Matrix4x4 vp = project * view[i];
            prefiter_matrix_->Update(&vp);

            Vec4f roughness_for_mip;
            roughness_for_mip.x = (float)j / (mip_levels - 1);
            roughness->Update(&roughness_for_mip);
            randiance_target->AttachColor(AttachmentPoint::kColor0, radiance_, i, j);

            MaterialGuard mat_guard(gfx_, prefilter_material_.get());
            RenderTargetGuard rt_guard(randiance_target.get());

            skybox_cube_->Render(gfx_);
        }
    }
}

}
}
