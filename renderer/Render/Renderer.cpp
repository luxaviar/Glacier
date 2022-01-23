#include "Renderer.h"
#include <memory>
#include <assert.h>
#include <algorithm>
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui.h"
#include "Math/Util.h"
#include "Render/Graph/PassNode.h"
#include "Render/Graph/ResourceEntry.h"
#include "Render/Mesh/MeshRenderer.h"
#include "Camera.h"
#include "Math/Vec3.h"
#include "Math/Mat4.h"
#include "Core/GameObject.h"
#include "Core/ObjectManager.h"
#include "CascadedShadowManager.h"
#include "LightManager.h"
#include "Core/Scene.h"
#include "physics/World.h"
#include "Input/Input.h"
#include "Common/BitUtil.h"
#include "Render/Image.h"
#include "Render/Base/Renderable.h"
#include "Render/Base/RenderTarget.h"
#include "Render/Base/SamplerState.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/SwapChain.h"
#include "Inspect/Profiler.h"

namespace glacier {
namespace render {

Renderer::Renderer(GfxDriver* gfx, MSAAType msaa) :
    msaa_(msaa),
    gfx_(gfx),
    editor_(gfx),
    post_process_manager_(gfx)
{
    if (msaa_ != MSAAType::kNone) {
        gfx_->CheckMSAA(msaa_, sample_count_, quality_level_);
        assert(sample_count_ <= 8);

        msaa_ = (MSAAType)sample_count_;
    }

    stats_ = std::make_unique<PerfStats>(gfx);

    csm_manager_ = std::make_shared<CascadedShadowManager>(gfx, 1024, std::vector<float>{ 15, 40, 70, 100 });

    InitRenderTarget();
    InitMaterial();

    InitPostProcess();
    InitMSAA();
}

void Renderer::InitPostProcess() {
    auto tonemapping_mat = std::make_shared<PostProcessMaterial>("tone mapping", TEXT("ToneMapping"));
    auto desc = PostProcess::Description()
        .SetSrc(intermediate_target_->GetColorAttachment(AttachmentPoint::kColor0))
        .SetDst(gfx_->GetSwapChain()->GetRenderTarget())
        .SetMaterial(tonemapping_mat);

    post_process_manager_.Push(desc);
}

void Renderer::InitMSAA() {
    RasterStateDesc rs;
    rs.depthFunc = CompareFunc::kAlways;

    auto vert_shader = gfx_->CreateShader(ShaderType::kVertex, TEXT("MSAAResolve"));

    std::array<std::pair<MSAAType, std::string>, 3> msaa_types = { { {MSAAType::k2x, "2"}, {MSAAType::k4x, "4"}, {MSAAType::k8x, "8"} } };
    for (auto& v : msaa_types) {
        auto pixel_shader = gfx_->CreateShader(ShaderType::kPixel, TEXT("MSAAResolve"), "main_ps", { {"MSAASamples_", v.second.c_str()}, {nullptr, nullptr} });
        auto program = gfx_->CreateProgram("MSAA");
        program->SetShader(vert_shader);
        program->SetShader(pixel_shader);

        auto mat = std::make_shared<Material>("msaa resolve", program);
        mat->GetTemplate()->SetRasterState(rs);

        mat->SetProperty("depth_buffer", render_target_->GetDepthStencil());
        mat->SetProperty("color_buffer", render_target_->GetColorAttachment(AttachmentPoint::kColor0));
        msaa_resolve_mat_[glacier::log2((uint32_t)v.first)] = mat;
    }
}

void Renderer::Setup() {
    if (init_) return;

    init_ = true;
    Gizmos::Instance()->Setup(gfx_);
    LightManager::Instance()->Setup(gfx_, this);
}

void Renderer::OnResize(uint32_t width, uint32_t height) {
    auto swapchain = gfx_->GetSwapChain();
    if (swapchain->GetWidth() == width && swapchain->GetHeight() == height) {
        return;
    }

    swapchain->OnResize(width, height);
    auto backbuffer_depth = swapchain->GetRenderTarget()->GetDepthStencil();

    auto rt_tex = render_target_->GetColorAttachment(AttachmentPoint::kColor0);
    rt_tex->Resize(width, height);

    render_target_->Resize(width, height);
    render_target_->AttachColor(AttachmentPoint::kColor0, rt_tex);
    if (msaa_ == MSAAType::kNone) {
        render_target_->AttachDepthStencil(backbuffer_depth);
    }
    else {
        auto depth_tex_desc = Texture::Description()
            .SetDimension(width, height)
            .SetFormat(TextureFormat::kR24G8_TYPELESS)
            .SetCreateFlag(CreateFlags::kDepthStencil)
            .SetCreateFlag(CreateFlags::kShaderResource)
            .SetSampleDesc(sample_count_, quality_level_);
        auto depthstencil_texture = gfx_->CreateTexture(depth_tex_desc);
        depthstencil_texture->SetName(TEXT("msaa depth texture"));

        render_target_->AttachDepthStencil(depthstencil_texture);
    }

    rt_tex = intermediate_target_->GetColorAttachment(AttachmentPoint::kColor0);
    rt_tex->Resize(width, height);

    intermediate_target_->Resize(width, height);
    intermediate_target_->AttachColor(AttachmentPoint::kColor0, rt_tex);
    intermediate_target_->AttachDepthStencil(backbuffer_depth);
}

void Renderer::FilterVisibles() {
    PerfSample("Filter Visibles");
    auto main_camera = GetMainCamera();

    visibles_.clear();
    main_camera->Cull(RenderableManager::Instance()->GetList(), visibles_);

    //TODO: sort by depth?
    std::sort(visibles_.begin(), visibles_.end(), [](Renderable* a, Renderable* b) {
        auto template_a = a->GetMaterial()->GetTemplate()->id();
        auto template_b = b->GetMaterial()->GetTemplate()->id();
        if (template_a == template_b) {
            return a->GetMaterial()->id() < b->GetMaterial()->id();
        }

        return template_a < template_b;
    });
}

void Renderer::RestoreCommonBindings() {
    auto main_camera = GetMainCamera();
    gfx_->BindCamera(main_camera);
    render_target_->Bind(gfx_);
}

void Renderer::InitMaterial() {
    auto solid_mat = std::make_unique<Material>("solid", TEXT("Solid"), TEXT("Solid"));
    solid_mat->SetProperty("color", Color{ 1.0f,1.0f,1.0f, 1.0f });
    solid_mat->GetTemplate()->SetInputLayout(InputLayoutDesc{ InputLayoutDesc::Position3D });

    MaterialManager::Instance()->Add(std::move(solid_mat));
}

void Renderer::InitRenderTarget() {
    auto width = gfx_->GetSwapChain()->GetWidth();
    auto height = gfx_->GetSwapChain()->GetHeight();
    auto& swapchain_render_target = gfx_->GetSwapChain()->GetRenderTarget();
    auto backbuffer_depth_tex = swapchain_render_target->GetDepthStencil();

    auto colorframe_desc = Texture::Description()
        .SetDimension(width, height)
        .SetFormat(TextureFormat::kR16G16B16A16_FLOAT)
        .SetCreateFlag(CreateFlags::kRenderTarget)
        .SetSampleDesc(sample_count_, quality_level_);
    auto colorframe = gfx_->CreateTexture(colorframe_desc);
    colorframe->SetName(TEXT("linear render color texture"));

    render_target_ = gfx_->CreateRenderTarget(width, height);
    render_target_->AttachColor(AttachmentPoint::kColor0, colorframe);

    intermediate_target_ = gfx_->CreateRenderTarget(width, height);
    if (msaa_ == MSAAType::kNone) {
        render_target_->AttachDepthStencil(backbuffer_depth_tex);

        intermediate_target_->AttachColor(AttachmentPoint::kColor0, colorframe);
        intermediate_target_->AttachDepthStencil(backbuffer_depth_tex);
    }
    else {
        auto depth_tex_desc = Texture::Description()
            .SetDimension(width, height)
            .SetFormat(TextureFormat::kR24G8_TYPELESS)
            .SetCreateFlag(CreateFlags::kDepthStencil)
            .SetCreateFlag(CreateFlags::kShaderResource)
            .SetSampleDesc(sample_count_, quality_level_);
        auto depthstencil_texture = gfx_->CreateTexture(depth_tex_desc);
        depthstencil_texture->SetName(TEXT("msaa depth texture"));

        render_target_->AttachDepthStencil(depthstencil_texture);

        auto intermediate_color_desc = Texture::Description()
            .SetDimension(width, height)
            .SetCreateFlag(CreateFlags::kRenderTarget)
            .SetFormat(TextureFormat::kR16G16B16A16_FLOAT);
        auto intermediate_color = gfx_->CreateTexture(intermediate_color_desc);
        intermediate_color->SetName(TEXT("intermediate color buffer"));

        intermediate_target_->AttachColor(AttachmentPoint::kColor0, intermediate_color);
        intermediate_target_->AttachDepthStencil(backbuffer_depth_tex);
    }
}

Camera* Renderer::GetMainCamera() const {
    auto scene = SceneManager::Instance()->CurrentScene();
    if (!scene) {
        return nullptr;
    }

    return scene->GetMainCamera();
}

void Renderer::PreRender() {
    PerfSample("PreRender");
    stats_->PreRender();

    render_target_->Clear();
    
    if (msaa_ != MSAAType::kNone) {
        intermediate_target_->Clear();
    }

    RestoreCommonBindings();
    FilterVisibles();
}

void Renderer::PostRender() {
    Gizmos::Instance()->Clear();

    // present
    //gfx_->EndFrame();

    render_graph_.Reset();
}

void Renderer::Render() {
    auto& state = Input::GetJustKeyDownState();
    if (state.Tab) {
        show_gui_ = !show_gui_;
    }

    if (state.F4) {
        show_gizmo_ = !show_gizmo_;
    }

    if (state.F1) {
        show_imgui_demo_ = !show_imgui_demo_;
    }

    auto& present_render_target = gfx_->GetSwapChain()->GetRenderTarget();
    PreRender();

    auto& mouse = Input::Instance()->mouse();
    if (mouse.IsLeftDown() && !mouse.IsRelativeMode()) {
        editor_.Pick(mouse.GetPosX(), mouse.GetPosY(), GetMainCamera(), visibles_, present_render_target);
    }

    {
        PerfSample("Exexute render graph");
        render_graph_.Execute(this);
    }

    if (show_gizmo_) {
        PerfSample("Draw Gizmos");
        RestoreCommonBindings();
        physics::World::Instance()->OnDrawGizmos(true);
        editor_.DrawGizmos();
        Gizmos::Instance()->Render(gfx_);
    }

    render_target_->UnBind(gfx_);

    ResolveMSAA();
    
    {
        PerfSample("Post Process");
        post_process_manager_.Render();
    }

    present_render_target->Bind(gfx_);

    if (show_gui_) {
        PerfSample("Editor");
        editor_.DrawPanel();
    }

    if (show_imgui_demo_) {
        ImGui::ShowDemoWindow(&show_imgui_demo_);
    }

    if (state.F2) {
        GrabScreen();
    }

    if (state.F3) {
        csm_manager_->GrabShadowMap();
    }

    stats_->PostRender();

    gfx_->Present();

    PostRender();
}

void Renderer::ResolveMSAA() {
    PerfSample("Resolve MSAA");
    if (msaa_ == MSAAType::kNone) return;

    auto& mat = msaa_resolve_mat_[log2((uint32_t)msaa_)];
    RenderTargetBindingGuard rt_guard(gfx_, intermediate_target_.get());
    MaterialGuard mat_guard(gfx_, mat.get());

    gfx_->Draw(3, 0);
}

void Renderer::GrabScreen() {
    auto render_target = gfx_->GetSwapChain()->GetRenderTarget();
    auto width = render_target->width();
    auto height = render_target->height();

    auto& tex = render_target->GetColorAttachment(AttachmentPoint::kColor0);
    tex->ReadBackImage(0, 0, render_target->width(), render_target->height(), 0, 0,
        [width, height, this](const uint8_t* data, size_t raw_pitch) {
            Image image(width, height, false);

            for (int y = 0; y < height; ++y) {
                const uint8_t* texel = data;
                auto pixel = image.GetRawPtr<ColorRGBA32>(0, y);
                for (int x = 0; x < width; ++x) {
                    *pixel = *((const ColorRGBA32*)texel);
                    texel += sizeof(ColorRGBA32);
                    ++pixel;
                }
                data += raw_pitch;
            }

            image.Save(TEXT("screen_grab.png"), false);
        });
}

void Renderer::AddSolidPass() {
    //auto mat = MaterialManager::Instance()->Get("solid");
    //render_graph_.AddPass("solid",
    //    [&](PassNode& pass) {
    //    },
    //    [this, mat](Renderer* renderer, const PassNode& pass) {
    //        MaterialGuard guard(renderer->driver(), mat);
    //        pass.Render(renderer, visibles_);
    //    });
}

void Renderer::AddShadowPass() {
    render_graph_.AddPass("shadow",
        [&](PassNode& pass) {
        },
        [this](Renderer* renderer, const PassNode& pass) {
            PerfSample("Shadow pass");

            auto light_mgr = LightManager::Instance();
            auto main_camera_ = GetMainCamera();
            LightManager::Instance()->SortLight(main_camera_->ViewCenter());
            auto main_light = light_mgr->GetMainLight();
            if (!main_light || !main_light->HasShadow()) return;

            csm_manager_->Render(main_camera_, visibles_, main_light);

            renderer->RestoreCommonBindings();
        });
}

void Renderer::AddCubeShadowMap(GfxDriver* gfx, OldPointLight& light) {
    static constexpr UINT size = 1000;
    std::array<std::tuple<Vec3f, Vec3f>, 6> param = { {
        // +x
        {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        // -x
        {{-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        // +y
        {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        // -y
        {{0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        // +z
        {{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        // -z
        {{0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
    }};

    std::array<Camera*, 6> shadow_cameras;
    for (int i = 0; i < 6; ++i) {
        auto& go = GameObject::Create("cube map shadow camera");
        go.DontDestroyOnLoad(true);
        auto camera = go.AddComponent<Camera>(CameraType::kPersp);
        camera->LookAt(std::get<0>(param[i]), std::get<1>(param[i]));
        camera->fov(90.0f);
        camera->aspect(1.0f);
        shadow_cameras[i] = camera;
    }

    auto desc1 = Texture::Description()
        .SetType(TextureType::kTextureCube)
        .SetDimension(size, size)
        .SetFormat(TextureFormat::kR32_FLOAT);

    auto shadow_map_cube = gfx->CreateTexture(desc1);
    shadow_map_cube->SetName(TEXT("shadow_map_cube texture"));

    auto desc2 = Texture::Description()
        .SetDimension(size, size)
        .SetFormat(TextureFormat::kR24G8_TYPELESS)
        .SetCreateFlag(CreateFlags::kDepthStencil)
        .SetCreateFlag(CreateFlags::kShaderResource);
    auto depthstencil_texture1 = gfx->CreateTexture(desc2);
    depthstencil_texture1->SetName(TEXT("shadow_map_cube depth texture"));

    auto shadow_mat = std::make_unique<Material>("cube shadow", TEXT("Shadow"), TEXT("Shadow"));

    auto shadow_mat_ptr = shadow_mat.get();
    MaterialManager::Instance()->Add(std::move(shadow_mat));

    render_graph_.AddPass<std::vector<std::shared_ptr<RenderTarget>>>("shadow",
        [&](std::vector<std::shared_ptr<RenderTarget>>& shadow_map_rt, PassNode& pass) {
            shadow_map_rt.reserve(6);

            for (int i = 0; i < 6; ++i) {
                auto rt = gfx->CreateRenderTarget(shadow_map_cube->width(), shadow_map_cube->height());
                shadow_map_rt.push_back(rt);
                rt->AttachColor(AttachmentPoint::kColor0, shadow_map_cube, i);
                rt->AttachDepthStencil(depthstencil_texture1);
            }
        },
        [&](Renderer* renderer, std::vector<std::shared_ptr<RenderTarget>>& shadow_map_rt, PassNode& pass) {
            auto light_pos = light.pos();
            auto gfx = renderer->driver();

            //auto shadow_cubemap
            for (size_t i = 0; i < 6; i++)
            {
                auto& rt = shadow_map_rt[i];
                rt->Clear();
                rt->Bind(gfx);

                auto camera = shadow_cameras[i];
                camera->position({ light_pos.x, light_pos.y, light_pos.z });
                gfx->BindCamera(camera);

                //MaterialGuard guard(gfx, shadow_mat_ptr);
                for (auto o : visibles_) {
                    o->Render(gfx, shadow_mat_ptr);
                }
            }

            auto& rt = shadow_map_rt[5];
            //for binding as PSV, tex can't be render target simultaneously
            rt->UnBind(gfx);

            renderer->RestoreCommonBindings();
        });

    render_graph_.AddPass("solid",
        [&](PassNode& pass) {
        },
        [this](Renderer* renderer, const PassNode& pass) {
            pass.Render(renderer, visibles_);
        });

    auto shadow_space_tx = gfx->CreateConstantBuffer<Matrix4x4>();

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kBorder;
    ss.filter = FilterMode::kCmpBilinear;
    ss.comp = CompareFunc::kLessEqual;

    SamplerState ss1;
    ss1.warpU = ss1.warpV = WarpMode::kClamp;
    ss.filter = FilterMode::kAnisotropic;

    /// FIXME
    auto phong_mat = std::make_unique<Material>("lambert_cube");
    phong_mat->SetProperty("shadow_trans", shadow_space_tx);
    phong_mat->SetProperty("shadow_map", shadow_map_cube);
    phong_mat->GetTemplate()->SetProperty("sampler1", ss);
    phong_mat->GetTemplate()->SetProperty("sampler2", ss1);
    phong_mat->GetTemplate()->SetInputLayout(Mesh::kDefaultLayout);

    auto phong_mat_ptr = phong_mat.get();
    MaterialManager::Instance()->Add(std::move(phong_mat));

    render_graph_.AddPass("phong",
        [&](PassNode& pass) {
        },
        [&](Renderer* renderer, const PassNode& pass) {
            //TODO: gather light list
            auto main_camera_ = GetMainCamera();
            light.Bind(main_camera_->view());

            //transform matrix: world -> shadow
            auto light_pos = light.pos();
            auto t = Matrix4x4::Translate({ -light_pos.x, -light_pos.y, -light_pos.z });
            shadow_space_tx->Update(&t);

            MaterialGuard guard(renderer->driver(), phong_mat_ptr);
            pass.Render(renderer, visibles_);
        });
}

}
}
