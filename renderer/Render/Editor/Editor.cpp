#include "Editor.h"
#include <map>
#include "imgui/imgui.h"
#include "Core/GameObject.h"
#include "Core/ObjectManager.h"
#include "Render/Mesh/MeshRenderer.h"
#include "Render/Graph/PassNode.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/Renderer.h"
#include "Physics/World.h"
#include "Render/Base/RenderTarget.h"
#include "Render/Base/ConstantBuffer.h"
#include "Render/Base/Sampler.h"

namespace glacier {
namespace render {

Editor::Editor(GfxDriver* gfx) : width_(gfx->width()), height_(gfx->height()), pick_(gfx) {
    
}

void Editor::Pick(int x, int y, Camera* camera, const std::vector<Renderable*>& visibles, std::shared_ptr<RenderTarget>& rt) {
    auto id = pick_.Detect(x, y, camera, visibles, rt);

    selected_go_ = nullptr;
    if (id > 0) {
        auto mr = RenderableManager::Instance()->Find(id);
        if (mr) {
            selected_go_ = mr->game_object();
        }
    }
}

void Editor::DoFrame() {
    DrawScenePanel();
    DrawInspectorPanel();
}

void Editor::DrawInspectorPanel() {
    if (!selected_go_) return;

    selected_go_->DrawSelectedGizmos();

    ImGui::SetNextWindowPos(ImVec2(width_ * 0.775f, height_ * 0.05f), ImGuiCond_Once);// ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(width_ * 0.20f, height_ * 0.7f), ImGuiCond_Once);// ImGuiCond_FirstUseEver);

    ImGuiWindowFlags window_flags = 0;
    if (ImGui::Begin("Inspector", nullptr, window_flags)) {
        if (ImGui::RadioButton("Active", selected_go_->IsActive())) {
            if (selected_go_->IsActive()) {
                selected_go_->Deactivate();
            }
            else {
                selected_go_->Activate();
            }
        }
        selected_go_->DrawInspector();
    }

    ImGui::End();
}

void Editor::DrawScenePanel() {
    ImGui::SetNextWindowPos(ImVec2(width_ * 0.015f, height_ * 0.05f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(width_ * 0.2f, height_ * 0.7f), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags window_flags = 0;
    if (!ImGui::Begin("Scene Hierarchy", nullptr, window_flags)) {
        // Early out if the window is collapsed, as an optimization.
        ImGui::End();
        return;
    }

    uint32_t selected = selected_go_ ? selected_go_->id() : 0;
    auto& list = GameObjectManager::Instance()->GetSceneNodeList();
    for (auto o : list) {
        o->DrawSceneNode(selected);
    }

    if (selected > 0) {
        selected_go_ = GameObjectManager::Instance()->Find(selected);
    }

    ImGui::End();
}

void Editor::RegisterHighLightPass(GfxDriver* gfx, Renderer* renderer) {
    auto& render_graph = renderer->render_graph();

    auto outline_mat = std::make_shared<Material>("outline");
    auto solid_vs = gfx->CreateShader(ShaderType::kVertex, TEXT("Solid"));
    outline_mat->SetShader(solid_vs);

    render_graph.AddPass("outline mask",
        [&](PassNode& pass) {
            auto& rs = pass.raster_state();
            rs.depthWrite = false;
            rs.depthFunc = CompareFunc::kAlways;
            rs.stencilEnable = true;
            rs.stencilFunc = CompareFunc::kAlways;
            rs.depthStencilPassOp = StencilOp::kReplace;
        },
        [this, renderer, outline_mat](Renderer* renderer, const PassNode& pass) {
            if (!selected_go_) return;
            auto mr = selected_go_->GetComponent<MeshRenderer>();
            if (!mr) return;

            renderer->render_target()->BindDepthStencil();
            pass.Render(renderer, mr, outline_mat.get());
        });

    auto builder = Texture::Builder()
        .SetDimension(renderer->render_target()->width() / 2, renderer->render_target()->height() / 2)
        .SetFormat(TextureFormat::kR8G8B8A8_UNORM);

    auto outline_draw_tex = gfx->CreateTexture(builder);
    auto outline_draw_rt = gfx->CreateRenderTarget(outline_draw_tex->width(), outline_draw_tex->height());
    outline_draw_rt->AttachColor(AttachmentPoint::kColor0, outline_draw_tex);

    auto solid_mat = MaterialManager::Instance()->Get("solid");
    auto outline_solid_mat = std::make_shared<Material>(*solid_mat);

    //Color Color{ 1.0f, 0.4f, 0.4f, 1.0f };
    outline_solid_mat->SetProperty("outline_color", Color{ 1.0f, 0.4f, 0.4f, 1.0f }, ShaderType::kPixel, 0);

    render_graph.AddPass("outline draw",
        [&](PassNode& pass) {
            //pass.SetRenderTarget(outline_draw_rt);
        },
        [this, outline_draw_rt, outline_solid_mat](Renderer* renderer, const PassNode& pass) {
            if (!selected_go_) return;
            auto mr = selected_go_->GetComponent<MeshRenderer>();
            if (!mr) return;

            outline_draw_rt->Clear();
            outline_draw_rt->Bind();
            pass.Render(renderer, mr, outline_solid_mat.get());
        });

    SetKernelGauss(blur_param_, 4, 2.0);
    auto blur_param = gfx->CreateConstantBuffer<BlurParam>(blur_param_);
    auto blur_dir = gfx->CreateConstantBuffer<BlurDirection>(blur_dir_);

    auto builder1 = Texture::Builder()
        .SetDimension(renderer->render_target()->width() / 2, renderer->render_target()->height() / 2)
        .SetFormat(TextureFormat::kR8G8B8A8_UNORM);

    auto outline_htex = gfx->CreateTexture(builder1);
    auto outline_hrt = gfx->CreateRenderTarget(outline_htex->width(), outline_htex->height());
    outline_hrt->AttachColor(AttachmentPoint::kColor0, outline_htex);

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kMirror;
    ss.filter = FilterMode::kPoint;

    auto outline_hsample = gfx->CreateSampler(ss);

    //auto fs_vs = gfx->CreateShader(ShaderType::kVertex, TEXT("BlurOutline"));
    auto blur_ps = gfx->CreateShader(ShaderType::kPixel, TEXT("BlurOutline"));
    auto hblur_mat = std::make_shared<Material>("hightlight");
    //hblur_mat->SetShader(fs_vs);
    hblur_mat->SetShader(blur_ps);
    hblur_mat->SetProperty("blur_param", blur_param, ShaderType::kPixel, 0);
    hblur_mat->SetProperty("blur_dir", blur_dir, ShaderType::kPixel, 1);
    //hblur_mat->SetProperty("blur_tex", outline_draw_tex, ShaderType::kPixel, 0);
    hblur_mat->SetProperty("blur_sampler", outline_hsample, ShaderType::kPixel, 0);

    //auto quad = renderer->FullscreenQuadMesh();
    render_graph.AddPass("horizontal blur",
        [&](PassNode& pass) {
        },
        [this, outline_hrt, blur_dir, outline_draw_tex, hblur_mat](Renderer* renderer, const PassNode& pass) {
            if (!selected_go_) return;
            auto mr = selected_go_->GetComponent<MeshRenderer>();
            if (!mr) return;

            outline_hrt->ClearColor(AttachmentPoint::kColor0);
            blur_dir_.isHorizontal = true;
            blur_dir->Update(&blur_dir_);

            auto& mgr = renderer->GetPostProcessManager();
            mgr.Process(outline_draw_tex.get(), outline_hrt.get(), hblur_mat.get());

            //outline_hrt->Bind();
            //pass.Render(renderer, quad, hblur_mat.get());
        });

    SamplerState vss;
    vss.warpU = vss.warpV = WarpMode::kMirror;
    vss.filter = FilterMode::kPoint;

    auto v_sample = gfx->CreateSampler(vss);

    auto vblur_mat = std::make_shared<Material>(*hblur_mat);
    //vblur_mat->SetProperty("blur_tex", outline_htex, ShaderType::kPixel, 0);
    vblur_mat->SetProperty("blur_sampler", v_sample, ShaderType::kPixel, 0);

    RasterState rs;
    rs.depthWrite = false;
    rs.stencilEnable = true;
    rs.stencilFunc = CompareFunc::kNotEqual;
    rs.depthStencilPassOp = StencilOp::kKeep;
    rs.blendFunctionSrcRGB = BlendFunction::kSrcAlpha;
    rs.blendFunctionDstRGB = BlendFunction::kOneMinusSrcAlpha;

    render_graph.AddPass("vertical blur",
        [&](PassNode& pass) {
        },
        [this, rs, blur_dir, vblur_mat, outline_htex](Renderer* renderer, const PassNode& pass) {
            if (!selected_go_) return;
            auto mr = selected_go_->GetComponent<MeshRenderer>();
            if (!mr) return;

            blur_dir_.isHorizontal = false;
            blur_dir->Update(&blur_dir_);

            auto& mgr = renderer->GetPostProcessManager();
            mgr.Process(outline_htex.get(), renderer->render_target().get(), vblur_mat.get(), rs);

            //renderer->render_target()->Bind();
            //pass.Render(renderer, quad, vblur_mat.get());
        });
}

template<typename T>
static constexpr T gauss(T x, T sigma) noexcept {
    const auto ss = sigma * sigma;
    return ((T)1.0 / ::sqrt((T)2.0 * (T)math::kPI * ss)) * ::exp(-(x * x) / ((T)2.0 * ss));
}

void Editor::SetKernelGauss(BlurParam& param, int radius, float sigma) {
    const int nTaps = radius * 2 + 1;
    param.nTaps = nTaps;
    float sum = 0.0f;
    for (int i = 0; i < nTaps; i++)
    {
        const auto x = float(i - radius);
        const auto g = gauss(x, sigma);
        sum += g;
        param.coefficients[i] = g;
    }
    for (int i = 0; i < nTaps; i++)
    {
        param.coefficients[i] = (float)param.coefficients[i] / sum;
    }
}

}
}