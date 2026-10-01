#include "Editor.h"
#include <algorithm>
#include <cstdarg>
#include <map>
#include <imgui.h>
#include "Core/GameObject.h"
#include "Core/ObjectManager.h"
#include "Render/Mesh/MeshRenderer.h"
#include "Render/Graph/PassNode.h"
#include "Render/Graph/RenderGraph.h"
#include "Render/Renderer.h"
#include "Physics/World.h"
#include "Render/Base/RenderTarget.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/SamplerState.h"
#include "Render/Base/SwapChain.h"
#include "Render/Base/Program.h"
#include "Render/Base/RenderTexturePool.h"
#include "../Image.h"
#include "Inspect/Profiler.h"
#include "Input/Input.h"
#include "Core/Behaviour.h"
#include "App.h"

namespace glacier {
namespace render {

Editor::Editor(GfxDriver* gfx) :
    gfx_(gfx),
    width_(gfx->GetSwapChain()->GetWidth()),
    height_(gfx->GetSwapChain()->GetHeight())
{
    color_buf_ = gfx->CreateConstantBuffer<Vec4f>();

    RasterStateDesc rs;
    rs.scissor = true;

    mat_ = std::make_shared<Material>("pick", TEXT("Solid"), TEXT("Solid"));
    mat_->GetProgram()->SetRasterState(rs);
    mat_->GetProgram()->SetInputLayout(Mesh::kDefaultLayout);
    mat_->SetProperty("color", color_buf_);
}

void Editor::OnResize(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
}

void Editor::Pick(CommandBuffer* cmd_buffer, int x, int y, Camera* camera, const std::vector<Renderable*>& visibles, std::shared_ptr<RenderTarget>& rt) {
    if (visibles.empty()) return;

    auto viewport = rt->viewport();
    Vec2i size{ 2, 2 };

    int minx = math::Round(x - size.x / 2.0f);
    int miny = math::Round(y - size.y / 2.0f);
    int maxx = math::Round(x + size.x / 2.0f);
    int maxy = math::Round(y + size.y / 2.0f);

    minx = std::max(minx, (int)viewport.top_left_x);
    miny = std::max(miny, (int)viewport.top_left_y);
    maxx = std::min(maxx, (int)(viewport.top_left_x + viewport.width - 1));
    maxy = std::min(maxy, (int)(viewport.top_left_y + viewport.height - 1));

    int sizex = maxx - minx;
    int sizey = maxy - miny;
    if (sizex <= 0 || sizey <= 0) return;

    ScissorRect rect{ minx, miny, maxx, maxy };
    rt->EnableScissor(rect);
    rt->Clear(cmd_buffer, { 0, 0, 0, 0 });
    rt->Bind(cmd_buffer);

    cmd_buffer->BindCamera(camera);
    {
        Vec4f encoded_id;
        for (auto o : visibles) {
            if (!o->IsActive() || !o->IsPickable()) continue;

            auto id = o->id();
            //a b g r uint32_t
            encoded_id.r = (id & 0xFF) / 255.0f;
            encoded_id.g = ((id >> 8) & 0xFF) / 255.0f;
            encoded_id.b = ((id >> 16) & 0xFF) / 255.0f;
            encoded_id.a = ((id >> 24) & 0xFF) / 255.0f;
            color_buf_->Update(&encoded_id);

            o->Render(cmd_buffer, mat_.get());
        }
    }
    rt->DisableScissor();

    auto tex = rt->GetColorAttachment(AttachmentPoint::kColor0);
    tex->ReadBackImage(cmd_buffer, minx, miny, sizex, sizey, 0, 0,
        [sizex, sizey, this](const uint8_t* data, size_t raw_pitch) {
            int pick_id = -1;
            int max_hit = 0;
            std::map<int, int> pick_item;

            for (int y = 0; y < sizey; ++y) {
                const uint8_t* texel = data;
                for (int x = 0; x < sizex; ++x) {
                    auto id = *((const int32_t*)texel);
                    texel += sizeof(uint32_t);

                    auto it = pick_item.find(id);
                    int hit = it == pick_item.end() ? 1 : it->second + 1;
                    pick_item[id] = hit;
                    if (hit > max_hit) {
                        pick_id = id;
                    }
                }
                data += raw_pitch;
            }

            selected_go_ = nullptr;
            if (pick_id > 0) {
                auto mr = RenderableManager::Instance()->Find(pick_id);
                if (mr) {
                    selected_go_ = mr->game_object();
                }
            }
        });
}

void Editor::Render(CommandBuffer* cmd_buffer) {
    DrawGizmos(cmd_buffer);
    DrawPanel();
}

void Editor::DrawGizmos(CommandBuffer* cmd_buffer) {
    PerfSample("Gizmos");

    if (selected_go_) {
        selected_go_->DrawSelectedGizmos();
    }

    if (enable_gizmos_) {
        if (scene_bvh_) {
            RenderableManager::Instance()->OnDrawGizmos();
        }

        if (physics_gizmos_) {
            physics::World::Instance()->OnDrawGizmos(true);
        }

        if (scene_gizmos_) {
            GameObjectManager::Instance()->DrawGizmos();
        }
    }

    Gizmos::Instance()->Render(cmd_buffer);
}

void Editor::DrawPanel() {
    PerfSample("Editor");

    auto& state = Input::GetJustKeyDownState();
    //tab hides the panels, but not while it is being typed into one of them
    if (state.Tab && !Input::IsTextInputActive()) {
        show_windows_ = !show_windows_;
    }

    //what the behaviours of the scene draw over it - the key hints of the demo
    //- is a foreground drawing of this frame, so it stays up whichever windows
    //are shown and takes no input of its own
    BehaviourManager::Instance()->DrawOverlay();

    if (show_windows_) {
        DrawMainMenu();
        if (show_scene_hierachy_) {
            DrawScenePanel();
        }

        if (show_inspector_) {
            DrawInspectorPanel();
        }

        if (show_profiler_) {
            DrawProfilerPanel();
        }

        if (show_imgui_demo_) {
            ImGui::ShowDemoWindow(&show_imgui_demo_);
        }
    }
}

void Editor::DrawInspectorPanel() {
    if (!selected_go_) return;

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

void Editor::DrawMainMenu() {
    auto& state = Input::GetJustKeyDownState();

    if (renderer_option_window_) {
        App::Self()->GetRenderer()->OptionWindow(&renderer_option_window_);
    }

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Options")) {
            ImGui::MenuItem("Renderer", NULL, &renderer_option_window_);
            //ImGui::Separator();
            //if (ImGui::MenuItem("Copy", "CTRL+C")) {}
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Windows")) {
            ImGui::MenuItem("Enabled", "Tab", &show_windows_);
            ImGui::MenuItem("Scene Hierarchy", "", &show_scene_hierachy_, show_windows_);
            ImGui::MenuItem("Inspector", "", &show_inspector_, show_windows_);
            ImGui::MenuItem("Statistics", "", &show_stats_, show_windows_);
            ImGui::MenuItem("CPU Profile", "", &show_profiler_, show_windows_);
            ImGui::MenuItem("IMGUI Demo", "", &show_imgui_demo_, show_windows_);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Gizmos")) {
            ImGui::MenuItem("Enabled", "", &enable_gizmos_);
            ImGui::Separator();
            ImGui::MenuItem("Scene", "", &scene_gizmos_, enable_gizmos_);
            ImGui::MenuItem("Physics", "", &physics_gizmos_, enable_gizmos_);
            ImGui::MenuItem("Scene BVH", "", &scene_bvh_, enable_gizmos_);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools")) {
            //the F2 key itself is handled by the app, so it works before the
            //first frame of the editor is up (and only captures once)
            if (ImGui::MenuItem("Capture Screen", "F2")) {
                App::Self()->GetRenderer()->CaptureScreen();
            }

            if (ImGui::MenuItem("Capture ShadowMap", "F3") || state.F3) {
                App::Self()->GetRenderer()->CaptureShadowMap();
            }

            //F4 itself is handled by the app, the way F2 is, so it writes the
            //profile even before the first frame of the editor is up
            if (ImGui::MenuItem("Write Profile", "F4")) {
                Profiler::Instance()->PrintFrame();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
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

    uint32_t cur_selected = selected_go_ ? selected_go_->id() : 0;
    uint32_t selected = cur_selected;
    auto& list = GameObjectManager::Instance()->GetSceneNodeList();
    for (auto o : list) {
        o->DrawSceneNode(selected);
    }

    if (selected != cur_selected) {
        selected_go_ = GameObjectManager::Instance()->Find(selected);
    }

    ImGui::End();
}

void Editor::RegisterHighLightPass(GfxDriver* gfx, Renderer* renderer) {
    auto& render_graph = renderer->render_graph();
    auto outline_mat = std::make_shared<Material>("outline", TEXT("Solid"), nullptr);

    RasterStateDesc outline_rs;
    outline_rs.depthWrite = false;
    outline_rs.depthFunc = CompareFunc::kAlways;
    outline_rs.stencilEnable = true;
    outline_rs.stencilFunc = CompareFunc::kAlways;
    outline_rs.depthStencilPassOp = StencilOp::kReplace;
    outline_mat->GetProgram()->SetRasterState(outline_rs);
    outline_mat->GetProgram()->SetInputLayout(Mesh::kDefaultLayout);
    
    render_graph.AddPass("outline mask",
        [&](PassNode& pass) {
        },
        [this, renderer, outline_mat](CommandBuffer* cmd_buffer, const PassNode& pass) {
            if (!selected_go_) return;
            auto mr = selected_go_->GetComponent<MeshRenderer>();
            if (!mr) return;

            renderer->GetLightingRenderTarget()->BindDepthStencil(cmd_buffer);
            pass.Render(cmd_buffer, mr, outline_mat.get());
        });

    auto outline_draw_tex = RenderTexturePool::Get(renderer->GetLightingRenderTarget()->width() / 2, renderer->GetLightingRenderTarget()->height() / 2);
    outline_draw_tex->SetName("Outline draw texture");

    auto outline_draw_rt = gfx->CreateRenderTarget(outline_draw_tex->width(), outline_draw_tex->height());
    outline_draw_rt->AttachColor(AttachmentPoint::kColor0, outline_draw_tex);

    auto solid_mat = MaterialManager::Instance()->Get("solid");
    auto outline_solid_mat = std::make_shared<Material>(*solid_mat);

    outline_solid_mat->SetProperty("color", Color{ 1.0f, 0.4f, 0.4f, 1.0f });

    render_graph.AddPass("outline draw",
        [&](PassNode& pass) {
        },
        [this, outline_draw_tex, outline_draw_rt, outline_solid_mat](CommandBuffer* cmd_buffer, const PassNode& pass) {
            if (!selected_go_) return;
            auto mr = selected_go_->GetComponent<MeshRenderer>();
            if (!mr) return;

            outline_draw_rt->Clear(cmd_buffer);
            RenderTargetGuard gurad(cmd_buffer, outline_draw_rt.get());

            pass.Render(cmd_buffer, mr, outline_solid_mat.get());
        });

    SetKernelGauss(blur_param_, 4, 2.0);
    auto blur_param = gfx->CreateConstantBuffer<BlurParam>(blur_param_, UsageType::kDefault);
    auto blur_dir = gfx->CreateConstantBuffer<BlurDirection>(blur_dir_);

    auto outline_htex = RenderTexturePool::Get(renderer->GetLightingRenderTarget()->width() / 2, renderer->GetLightingRenderTarget()->height() / 2);
    outline_htex->SetName("horizontal outline draw texture");

    auto outline_hrt = gfx->CreateRenderTarget(outline_htex->width(), outline_htex->height());
    outline_hrt->AttachColor(AttachmentPoint::kColor0, outline_htex);

    SamplerState ss;
    ss.warpU = ss.warpV = WarpMode::kMirror;
    ss.filter = FilterMode::kPoint;

    RasterStateDesc hblur_rs;
    hblur_rs.depthEnable = false;
    hblur_rs.depthWrite = true;
    hblur_rs.depthFunc = RasterStateDesc::kDefaultDepthFunc;
    
    auto hblur_mat = std::make_shared<PostProcessMaterial>("hightlight", TEXT("BlurOutline"));

    hblur_mat->GetProgram()->SetRasterState(hblur_rs);
    hblur_mat->SetProperty("Kernel", blur_param);
    hblur_mat->SetProperty("Control", blur_dir);
    hblur_mat->SetProperty("_point_clamp_sampler", ss);
    hblur_mat->SetProperty("_PostSourceTexture", outline_draw_tex);

    render_graph.AddPass("horizontal blur",
        [&](PassNode& pass) {
        },
        [this, outline_htex, outline_hrt, blur_dir, hblur_mat](CommandBuffer* cmd_buffer, const PassNode& pass) {
            if (!selected_go_) return;
            auto mr = selected_go_->GetComponent<MeshRenderer>();
            if (!mr) return;

            outline_hrt->ClearColor(cmd_buffer, AttachmentPoint::kColor0);
            blur_dir_.isHorizontal = true;
            blur_dir->Update(&blur_dir_);

            Renderer::PostProcess(cmd_buffer, outline_hrt, hblur_mat.get());
        });

    SamplerState vss;
    vss.warpU = vss.warpV = WarpMode::kMirror;
    vss.filter = FilterMode::kPoint;

    auto vblur_mat = std::make_shared<PostProcessMaterial>("hightlight", TEXT("BlurOutline"));
    vblur_mat->SetProperty("_point_clamp_sampler", vss);

    RasterStateDesc blur_rs;
    blur_rs.depthWrite = false;
    blur_rs.stencilEnable = true;
    blur_rs.depthFunc = CompareFunc::kAlways;
    blur_rs.stencilFunc = CompareFunc::kNotEqual;
    blur_rs.depthStencilPassOp = StencilOp::kKeep;
    blur_rs.blendFunctionSrcRGB = BlendFunction::kSrcAlpha;
    blur_rs.blendFunctionDstRGB = BlendFunction::kOneMinusSrcAlpha;
    vblur_mat->GetProgram()->SetRasterState(blur_rs);
    vblur_mat->SetProperty("Kernel", blur_param);
    vblur_mat->SetProperty("Control", blur_dir);
    vblur_mat->SetProperty("_PostSourceTexture", outline_htex);

    render_graph.AddPass("vertical blur",
        [&](PassNode& pass) {
        },
        [this, blur_dir, renderer, vblur_mat](CommandBuffer* cmd_buffer, const PassNode& pass) {
            if (!selected_go_) return;
            auto mr = selected_go_->GetComponent<MeshRenderer>();
            if (!mr) return;

            blur_dir_.isHorizontal = false;
            blur_dir->Update(&blur_dir_);

            Renderer::PostProcess(cmd_buffer, renderer->GetLightingRenderTarget(), vblur_mat.get());
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

namespace {

//the widths of the numbers that follow the name, as printf field widths; the
//header row is written with the same ones so the two line up
constexpr int kSelfChars = 6;   //"%6.2f"
constexpr int kTotalChars = 6;  //"%6.2f"
constexpr int kShareChars = 7;  //"%6.1f%%"
constexpr int kCallsChars = 5;  //"%5.0f"

//a node whose self time is this much of the frame is what the panel is for, so
//it is the one thing that is coloured
constexpr double kProfileHotShare = 0.2;
//a node that runs less often than every other frame does not get a row: a row
//that comes and goes is one more thing that flickers
constexpr double kProfileMinCalls = 0.5;

const ImVec4 kProfileNumberColor(0.75f, 0.75f, 0.78f, 1.0f);
const ImVec4 kProfileHotColor(1.0f, 0.55f, 0.40f, 1.0f);
const ImVec4 kProfileHeadColor(0.55f, 0.55f, 0.58f, 1.0f);

struct ProfileColumns {
    float name = 0.0f;
    float self = 0.0f;
    float total = 0.0f;
    float share = 0.0f;
    float calls = 0.0f;
};

//The numbers are the same width on every row, so where they start is a column
//of its own and the name gets whatever the window has left. That is what keeps
//a deep row and a shallow one reading the same.
ProfileColumns ProfileColumnLayout() {
    const float digit = ImGui::CalcTextSize("0").x;
    const float gap = digit;

    const float numbers = (kSelfChars + kTotalChars + kShareChars + kCallsChars) * digit + 3.0f * gap;
    const float width = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2.0f;

    ProfileColumns columns;
    columns.name = std::clamp(width - numbers, 70.0f, 220.0f);
    columns.self = columns.name;
    columns.total = columns.self + kSelfChars * digit + gap;
    columns.share = columns.total + kTotalChars * digit + gap;
    columns.calls = columns.share + kShareChars * digit + gap;
    return columns;
}

//a row of numbers, right aligned in fields of the widths above
void ProfileNumberRow(const ProfileColumns& columns, const ImVec4& color,
    double self, double total, double share, double calls)
{
    ImGui::SameLine(columns.self);
    ImGui::TextColored(color, "%6.2f", self);
    ImGui::SameLine(columns.total);
    ImGui::TextColored(color, "%6.2f", total);
    ImGui::SameLine(columns.share);
    ImGui::TextColored(color, "%6.1f%%", share);
    ImGui::SameLine(columns.calls);
    ImGui::TextColored(color, "%5.0f", calls);
}

bool ProfileNodeShown(const Profiler::Node& node) {
    return node.average().calls >= kProfileMinCalls;
}

void DrawProfilerNode(const Profiler::Node& node, double frame_ms, const ProfileColumns& columns) {
    auto profiler = Profiler::Instance();

    if (!ProfileNodeShown(node)) {
        //the node is not worth a row of its own, but one of its children may be
        for (auto* child : node.children()) {
            DrawProfilerNode(*child, frame_ms, columns);
        }

        return;
    }

    bool has_child = false;
    for (auto* child : node.children()) {
        if (ProfileNodeShown(*child)) {
            has_child = true;
            break;
        }
    }

    //the id of a row is the node itself and not its label, so numbers that
    //change every frame do not cost the row its open state
    auto flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!has_child) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    const bool open = ImGui::TreeNodeEx((const void*)&node, flags, "%s", node.name());

    const auto& average = node.average();
    const double self = profiler->AverageSelfMs(node);
    //what a row accounts for is its whole span, so the frame itself is 100% and
    //a child never reads higher than the node it sits under
    const double share = frame_ms > 0.0 ? average.total_ms / frame_ms * 100.0 : 0.0;
    const double self_share = frame_ms > 0.0 ? self / frame_ms : 0.0;

    if (ImGui::IsItemHovered()) {
        const auto& last = node.span(Profiler::Period::kLast);
        ImGui::SetTooltip("%s\nthe row is an average of the recent frames\n"
            "the frame that ended last: self %.3f ms, total %.3f ms, max %.3f ms, %u calls",
            node.name(), profiler->SelfTimeMs(node, Profiler::Period::kLast),
            last.total_ms(), last.max_ms(), last.calls);
    }

    ProfileNumberRow(columns, self_share >= kProfileHotShare ? kProfileHotColor : kProfileNumberColor,
        self, average.total_ms, share, average.calls);

    if (open && has_child) {
        for (auto* child : node.children()) {
            DrawProfilerNode(*child, frame_ms, columns);
        }

        ImGui::TreePop();
    }
}

}

void Editor::DrawProfilerPanel() {
    PerfSample("Profiler panel");

    auto profiler = Profiler::Instance();
    auto* frame_node = profiler->frame_node();
    if (!frame_node) {
        return;
    }

    //the panel is built in the middle of a frame, so the newest frame there is
    //a complete measurement of is the one that ended before this one
    const auto& frame_span = frame_node->span(Profiler::Period::kLast);
    if (frame_span.calls == 0) {
        return;
    }

    //both the frame and the rows are read averaged, so the shares below add up
    //to the frame and the numbers hold still enough to be read
    const double frame_ms = frame_node->average().total_ms;

    //the middle of the bottom half: the hierarchy owns the left of the window,
    //the inspector the right, the statistics box the bottom left corner and the
    //hint of a demo scene the top middle
    ImGui::SetNextWindowPos(ImVec2(width_ * 0.26f, height_ * 0.52f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(width_ * 0.46f, height_ * 0.46f), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("CPU Profile")) {
        ImGui::End();
        return;
    }

    const auto columns = ProfileColumnLayout();

    auto labeled = [&columns](const char* label, const char* tooltip, const char* fmt, ...) {
        ImGui::TextUnformatted(label);
        if (tooltip != nullptr && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", tooltip);
        }

        ImGui::SameLine(columns.name);
        va_list args;
        va_start(args, fmt);
        ImGui::TextV(fmt, args);
        va_end(args);
    };

    //the wall time of the loop iteration is what the frame rate is, and the
    //span the tree below measures is a part of it; what is left over is the loop
    //around the frame, the message pump mostly
    const double wall_ms = profiler->average_frame_ms();
    const double unprofiled_ms = wall_ms > frame_ms ? wall_ms - frame_ms : 0.0;

    labeled("frames", nullptr, "%u", profiler->frame_count() - 1);
    labeled("wall", "the whole loop iteration, end to end:\nthe message pump, the input, and the frame below\n"
        "the frame rate is this one",
        "%.2f ms  end to end, %.1f fps", wall_ms, profiler->frame_rate());
    labeled("profiled", "what the tree below adds up to.\n"
        "It is not pure cpu work: the waits for the gpu and for the swap chain\n"
        "happen inside it, and show up as begin frame, present and end frame",
        "%.2f ms  the span the tree below adds up to", frame_ms);
    labeled("unprofiled", "wall minus profiled: the cost of the loop around the frame",
        "%.2f ms  outside that span, the message pump mostly", unprofiled_ms);
    labeled("overhead", "what one Begin/End pair costs, measured at startup.\n"
        "It is subtracted from self, so a sample does not pay for being measured.\n"
        "The clock is a counter, and its smallest step is the second number:\n"
        "one sample costs less than a step, and a sample shorter than a step\n"
        "measures as no time at all",
        "%.0f ns per sample, the clock steps every %.0f ns",
        profiler->sample_overhead_ns(), profiler->clock_step_ns());

    ImGui::Separator();

    ImGui::Text("node");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("self is the node's own work: the spans of its children are taken out\n"
            "of it, and so is what the sampling itself cost\n"
            "share is the whole span of the node as a part of the frame\n"
            "hover a row for the frame that ended last");
    }
    ImGui::SameLine(columns.self); ImGui::TextColored(kProfileHeadColor, "%6s", "self");
    ImGui::SameLine(columns.total); ImGui::TextColored(kProfileHeadColor, "%6s", "total");
    ImGui::SameLine(columns.share); ImGui::TextColored(kProfileHeadColor, "%7s", "share");
    ImGui::SameLine(columns.calls); ImGui::TextColored(kProfileHeadColor, "%5s", "calls");

    for (auto* child : profiler->root()->children()) {
        DrawProfilerNode(*child, frame_ms, columns);
    }

    ImGui::Separator();

    if (ImGui::SmallButton("Write to log")) {
        profiler->PrintFrame();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("(F4)");

    ImGui::End();
}

}
}
