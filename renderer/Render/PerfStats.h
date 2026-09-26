#pragma once

#include "render/base/query.h"
#include "Inspect/timer.h"
#include "Inspect/statistic.h"

namespace glacier {
namespace render {

class GfxDriver;
class CommandBuffer;

class PerfStats {
public:
    PerfStats(GfxDriver* gfx);

    void PreRender(CommandBuffer* cmd_buffer);
    void PostRender(CommandBuffer* cmd_buffer, bool show_stats);

    void DrawStatsPanel(CommandBuffer* cmd_buffer);

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
    Timer timer_;

    Statistic gpu_stats_;
    Statistic cpu_stats_;
};

}
}