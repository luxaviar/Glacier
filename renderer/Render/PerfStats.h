#pragma once

#include "render/base/query.h"
#include "Inspect/timer.h"
#include "Inspect/statistic.h"

namespace glacier {
namespace render {

class GfxDriver;

class PerfStats {
public:
    PerfStats(GfxDriver* gfx);

    void PreRender();
    void PostRender(bool show_stats);

    void DrawStatsPanel();

protected:
    void Reset();

    double cpu_time_ = 0.0;
    double gpu_time_ = 0.0;
    double accum_time_ = 0.0;

    std::shared_ptr<Query> frame_query_;
    std::shared_ptr<Query> primitiv_query_;
    Timer timer_;

    Statistic gpu_stats_;
    Statistic cpu_stats_;
};

}
}