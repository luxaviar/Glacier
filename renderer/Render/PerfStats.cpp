#include "PerfStats.h"
#include <imgui.h>
#include <algorithm>
#include "Render/Base/GfxDriver.h"
#include "Render/Base/SwapChain.h"
#include "Render/Base/CommandBuffer.h"
#include "Render/Base/CommandQueue.h"
#include "Common/Log.h"
#include "Inspect/Profiler.h"

namespace glacier {
namespace render {

GpuPassTimer::GpuPassTimer(GfxDriver* gfx) :
    gfx_(gfx),
    query_(gfx->CreateQuery(QueryType::kTimeStamp, kMaxPasses))
{
}

void GpuPassTimer::BeginFrame(CommandBuffer* cmd_buffer) {
    // the frame before this one resolved its pairs into its own slot; the fence
    // of the last submission of this command buffer is that frame's, so it says
    // whether the numbers are there yet
    if (pending_slot_ < Query::kFrameSlots) {
        const uint64_t fence = cmd_buffer != nullptr ? cmd_buffer->submitted_fence() : 0;
        auto queue = gfx_->GetCommandQueue(CommandBufferType::kDirect);

        if (fence != 0 && queue->IsFenceComplete(fence) && ReadSlot(pending_slot_)) {
            pending_slot_ = Query::kFrameSlots;
        }
    }

    recorded_.clear();
}

void GpuPassTimer::BeginPass(const char* name, CommandBuffer* cmd_buffer) {
    if (recorded_.size() >= kMaxPasses) {
        if (!warned_capacity_) {
            LOG_WARN("the gpu pass timer holds {} passes and the frame ran more; the rest is not timed",
                kMaxPasses);
            warned_capacity_ = true;
        }

        open_.push_back(kNoPass);
        return;
    }

    if (!query_->BeginTimestamp(cmd_buffer, (uint32_t)recorded_.size())) {
        open_.push_back(kNoPass);
        return;
    }

    // a pass may bracket stages of its own - the render graph brackets the GTAO
    // pass and GTAO brackets its trace and its filters - so what is open when
    // this one starts is what tells a pass of the frame from a stage of one
    PassTiming entry;
    entry.name = name;
    entry.depth = (uint32_t)open_.size();
    recorded_.push_back(entry);
    open_.push_back((uint32_t)recorded_.size() - 1);
}

void GpuPassTimer::EndPass(CommandBuffer* cmd_buffer) {
    if (open_.empty()) {
        return;
    }

    const uint32_t index = open_.back();
    open_.pop_back();

    if (index == kNoPass) {
        return;
    }

    query_->EndTimestamp(cmd_buffer, index);
}

void GpuPassTimer::EndFrame(CommandBuffer* cmd_buffer) {
    // a pass that was left open - the frame ended inside it - is closed here
    while (!open_.empty()) {
        EndPass(cmd_buffer);
    }

    if (recorded_.empty()) {
        return;
    }

    if (query_->ResolveTimestampRange(cmd_buffer, slot_, 0, (uint32_t)recorded_.size())) {
        slots_[slot_] = recorded_;
        pending_slot_ = slot_;
    }

    slot_ = (slot_ + 1) % Query::kFrameSlots;
}

bool GpuPassTimer::ReadSlot(uint32_t slot) {
    auto& stored = slots_[slot];

    for (uint32_t i = 0; i < (uint32_t)stored.size(); ++i) {
        double seconds = 0.0;
        if (!query_->GetTimestamp(slot, i, seconds)) {
            return false;
        }

        stored[i].ms = seconds * 1000.0;
    }

    timings_ = stored;
    return true;
}

double GpuPassTimer::total_ms() const {
    double total = 0.0;
    for (const auto& pass : timings_) {
        // the top level only: the stages of a pass are inside it already
        if (pass.depth == 0) {
            total += pass.ms;
        }
    }

    return total;
}

void PerfStats::PrintPassTimings() {
    const auto& timings = pass_timer_.timings();
    const double total = pass_timer_.total_ms();
    const double frame_gpu = gpu_time_ * 1000.0;

    LOG_LOG("--- gpu passes: the newest frame whose timings completed (frame gpu {:.3f} ms) ---",
        frame_gpu);
    LOG_LOG("{:<28} {:>9} {:>8}", "pass", "gpu ms", "%gpu");

    for (const auto& pass : timings) {
        LOG_LOG("{:<28} {:>9.3f} {:>7.1f}%", pass.name, pass.ms,
            frame_gpu > 0.0 ? pass.ms / frame_gpu * 100.0 : 0.0);
    }

    // the passes do not cover the whole frame: the frame query also spans the
    // editor overlay and the present of the frame, which are not bracketed
    LOG_LOG("{:<28} {:>9.3f} {:>7.1f}%", "sum of the passes", total,
        frame_gpu > 0.0 ? total / frame_gpu * 100.0 : 0.0);
    LOG_LOG("{:<28} {:>9.3f}", "frame gpu", frame_gpu);
}

PerfStats::PerfStats(GfxDriver* gfx) :
    pass_timer_(gfx)
{
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
    pass_timer_.BeginFrame(cmd_buffer);
}

void PerfStats::PostRender(CommandBuffer* cmd_buffer, bool show_stats) {
    frame_query_->End(cmd_buffer);
    primitiv_query_->End(cmd_buffer);
    pass_timer_.EndFrame(cmd_buffer);
    auto elapsed_time = timer_.DeltaTime();
    timer_.Mark();

    cpu_stats_.Sample(elapsed_time);

    auto result = frame_query_->GetQueryResult(cmd_buffer);
    if (result.is_valid) {
        gpu_stats_.Sample(result.elapsed_time);
    }

    //what the frame drew is read here rather than where it is shown, so that
    //whoever shows it does not have to own a command buffer of the frame
    auto primitives = primitiv_query_->GetQueryResult(cmd_buffer);
    if (primitives.is_valid) {
        vertices_ = primitives.vertices_rendered;
        primitives_ = primitives.primitives_rendered;
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

    //what the frame was held up by: the frame flush is gone, so what is left is
    //the frame contexts and the swapchain keeping the cpu from running further
    //ahead than the frames in flight allow (see GfxDriver::EndFrame)
    ImGui::Text("Wait:"); ImGui::SameLine(kLabelWidth);
    ImGui::Text("%.4fms", Profiler::Instance()->FrameWaitMs());

    //the counts of the frame were read by PostRender, so the panel only shows
    //them; the queries belong to the frame, not to the window
    ImGui::Text("Render Vertices:"); ImGui::SameLine(kLabelWidth);
    ImGui::Text("%llu", vertices_);

    ImGui::Text("Render Primitives:"); ImGui::SameLine(kLabelWidth);
    ImGui::Text("%llu", primitives_);

    ImGui::End();
}

}
}
