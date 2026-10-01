#pragma once

#include "render/base/query.h"
#include "Inspect/timer.h"
#include "Inspect/statistic.h"
#include "Common/Uncopyable.h"
#include "Render/Graph/PassObserver.h"
#include <vector>

namespace glacier {
namespace render {

class GfxDriver;
class CommandBuffer;

// One pass of a frame, as the GPU timed it. The name is a static string - the
// name of a pass of the render graph or one of the stages the renderer brackets
// itself - so the table never owns it.
struct PassTiming {
    const char* name = "";
    double ms = 0.0;
    // How deep the pass sits in the frame: a pass of the render graph is at 0,
    // a stage it brackets itself is at 1. Only the top level adds up to the
    // frame, which is what the sum printed with the table counts.
    uint32_t depth = 0;
};

// Per pass GPU times, from timestamp queries.
//
// The renderer brackets every pass it runs. The timer resolves the pairs of the
// frame in one call at the end of it, and reads them back a frame later, once
// the frame they belong to is known to have completed: a frame that runs ahead
// of the GPU keeps the table it had rather than reading half written numbers.
// Results of Query::kFrameSlots frames are kept apart for that reason.
class GpuPassTimer : public PassObserver {
public:
    // Timestamp pairs the timer can hold; a frame that records more passes than
    // this drops the rest (and says so once)
    static constexpr uint32_t kMaxPasses = 32;

    GpuPassTimer(GfxDriver* gfx);

    void BeginPass(const char* name, CommandBuffer* cmd_buffer) override;
    void EndPass(CommandBuffer* cmd_buffer) override;

    // Begins a frame: reads the frame before it back if the GPU finished it
    void BeginFrame(CommandBuffer* cmd_buffer);
    // Ends it: resolves what the frame recorded into its own slot
    void EndFrame(CommandBuffer* cmd_buffer);

    // The passes of the newest frame whose timings are complete, in the order the
    // frame ran them; a stage of a pass follows the pass itself
    const std::vector<PassTiming>& timings() const { return timings_; }
    // The top level passes only, so a pass and the stages it brackets are not
    // counted twice
    double total_ms() const;

private:
    bool ReadSlot(uint32_t slot);

    GfxDriver* gfx_;
    std::shared_ptr<Query> query_;
    // being recorded by the frame that is running
    std::vector<PassTiming> recorded_;
    // what each frame slot holds, until it is read back
    std::vector<PassTiming> slots_[Query::kFrameSlots];
    std::vector<PassTiming> timings_;
    uint32_t slot_ = 0;
    uint32_t pending_slot_ = Query::kFrameSlots;
    // the passes whose begin timestamp is written and whose end is not, the
    // innermost last; a pass that could not be timed is held as kNoPass so its
    // EndPass still pairs with its BeginPass
    static constexpr uint32_t kNoPass = 0xFFFFFFFF;
    std::vector<uint32_t> open_;
    bool warned_capacity_ = false;
};

// Brackets one stage, for the stages the renderer runs outside the render graph:
//
//     GpuPassScope pass(*stats_, "TAA", cmd_buffer);
//
class GpuPassScope : private Uncopyable {
public:
    GpuPassScope(GpuPassTimer& timer, const char* name, CommandBuffer* cmd_buffer) :
        timer_(timer),
        cmd_buffer_(cmd_buffer)
    {
        timer_.BeginPass(name, cmd_buffer_);
    }

    ~GpuPassScope() {
        timer_.EndPass(cmd_buffer_);
    }

private:
    GpuPassTimer& timer_;
    CommandBuffer* cmd_buffer_;
};

class PerfStats {
public:
    PerfStats(GfxDriver* gfx);

    void PreRender(CommandBuffer* cmd_buffer);
    void PostRender(CommandBuffer* cmd_buffer, bool show_stats);

    void DrawStatsPanel(CommandBuffer* cmd_buffer);

    // Splits the GPU time of a frame into the passes that make it up; the
    // renderer brackets the passes with it (see GpuPassTimer)
    GpuPassTimer& pass_timer() { return pass_timer_; }
    // Writes the table of the newest complete frame to the log, which is where
    // F4 puts it next to the profile tree
    void PrintPassTimings();

    //What the last frames drew, averaged over a second, for whoever shows it:
    //the panel of the editor and the hint of the demo scene both want it. The
    //GPU time and the counts of the frame come out of queries, so they belong to
    //the frame that was rendered before the one being drawn.
    double cpu_time() const { return cpu_time_; }
    double gpu_time() const { return gpu_time_; }
    uint64_t vertices() const { return vertices_; }
    uint64_t primitives() const { return primitives_; }

protected:
    void Reset();

    double cpu_time_ = 0.0;
    double gpu_time_ = 0.0;
    double accum_time_ = 0.0;
    uint64_t vertices_ = 0;
    uint64_t primitives_ = 0;

    std::shared_ptr<Query> frame_query_;
    std::shared_ptr<Query> primitiv_query_;
    GpuPassTimer pass_timer_;
    Timer timer_;

    Statistic gpu_stats_;
    Statistic cpu_stats_;
};

}
}
