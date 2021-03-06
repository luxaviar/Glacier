#include "PerfStats.h"
#include "imgui/imgui.h"
#include "Render/Base/GfxDriver.h"
#include "Render/Base/SwapChain.h"
#include "Common/Log.h"

namespace glacier {
namespace render {

PerfStats::PerfStats(GfxDriver* gfx) {
    frame_query_ = std::move(gfx->CreateQuery(QueryType::kTimeStamp, 3));
    primitiv_query_ = std::move(gfx->CreateQuery(QueryType::kPipelineStatistics, 3));
}

void PerfStats::Reset() {
    accum_time_ = 0.0;
    gpu_stats_.Reset();
    cpu_stats_.Reset();
}

void PerfStats::PreRender(CommandBuffer* cmd_buffer) {
    frame_query_->Begin(cmd_buffer);
    primitiv_query_->Begin(cmd_buffer);
}

void PerfStats::PostRender(CommandBuffer* cmd_buffer, bool show_stats) {
    frame_query_->End(cmd_buffer);
    primitiv_query_->End(cmd_buffer);
    auto elapsed_time = timer_.DeltaTime();
    timer_.Mark();

    cpu_stats_.Sample(elapsed_time);

    auto result = frame_query_->GetQueryResult(cmd_buffer);
    if (result.is_valid) {
        gpu_stats_.Sample(result.elapsed_time);
    }

    accum_time_ += elapsed_time;
    if (accum_time_ > 1.0) {
        gpu_time_ = gpu_stats_.average();
        cpu_time_ = cpu_stats_.average();
        Reset();
    }

    if (show_stats) {
        DrawStatsPanel(cmd_buffer);
    }
}

void PerfStats::DrawStatsPanel(CommandBuffer* cmd_buffer) {
    auto gfx = GfxDriver::Get();
    auto width_ = gfx->GetSwapChain()->GetWidth();
    auto height_ = gfx->GetSwapChain()->GetHeight();

    auto h = height_ * 0.2f;
    auto w = width_ * 0.2f;
    auto margin = width_ * 0.015f;
    ImGui::SetNextWindowPos(ImVec2(margin, height_ - margin - h), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
    if (!ImGui::Begin("Statistics", nullptr, window_flags)) {
        ImGui::PopStyleColor();
        // Early out if the window is collapsed, as an optimization.
        ImGui::End();
        return;
    }
    ImGui::PopStyleColor();

    auto elapsed_time = cpu_time_ == 0.0 ? 0.01 : cpu_time_;
    auto fps = 1.0f / elapsed_time;

    constexpr int kLabelWidth = 140;
    //ImGui::AlignTextToFramePadding();
    ImGui::Text("FPS:"); ImGui::SameLine(kLabelWidth);
    ImGui::Text("%d", (int)fps);

    //ImGui::AlignTextToFramePadding();
    ImGui::Text("Frame Time:"); ImGui::SameLine(kLabelWidth);
    ImGui::Text("%.4fms", elapsed_time * 1000.0f);

    elapsed_time = gpu_time_;
    if (elapsed_time == 0.0) {
        auto result = frame_query_->GetQueryResult(cmd_buffer);
        if (result.is_valid) {
            elapsed_time = result.elapsed_time;
        }
    }
    //ImGui::AlignTextToFramePadding();
    ImGui::Text("GPU Frame Time:"); ImGui::SameLine(kLabelWidth);
    ImGui::Text("%.4fms", elapsed_time * 1000.0f);

    auto result = primitiv_query_->GetQueryResult(cmd_buffer);

    ImGui::Text("Render Vertices:"); ImGui::SameLine(kLabelWidth);
    ImGui::Text("%llu", result.is_valid ? result.vertices_rendered : 0);

    ImGui::Text("Render Primitives:"); ImGui::SameLine(kLabelWidth);
    ImGui::Text("%llu", result.is_valid ? result.primitives_rendered : 0);
    
    ImGui::End();
}

}
}
