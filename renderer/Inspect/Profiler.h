#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>
#include "Common/Uncopyable.h"
#include "Common/Singleton.h"

namespace glacier {

// Hierarchical CPU sampling profiler.
//
// A sample has to cost far less than what it measures, otherwise the profile
// describes the profiler. The sampling path is therefore kept down to a clock
// read, a child lookup that almost always hits the child found last time, and a
// pointer store:
//
//   * nodes are allocated once, from an arena, and never move, so the tree can
//     be walked with raw pointers and resolving a name never allocates;
//   * the children of a node sit in one contiguous block;
//   * time is QueryPerformanceCounter read directly, and it stays in ticks
//     until a report is printed. Sampling through std::chrono turned every
//     measurement into nanoseconds on the frame it was measuring.
//
// Statistics are collected over the whole run and over single frames at the
// same time. A run long average is not enough on its own: the first frames of
// the process carry every shader compile and asset load of the run, and they
// hide what a steady frame costs, which is the number worth having.
//
// Single threaded on purpose: the engine samples from the main thread only, and
// a sample taken on a worker thread would race on current_node_.
class Profiler : public Singleton<Profiler> {
public:
    // QueryPerformanceCounter ticks
    using Ticks = int64_t;
    static constexpr Ticks kMaxTicks = std::numeric_limits<Ticks>::max();

    // A sample nested deeper than this is timed into the node already on the
    // stack, so a runaway recursion cannot grow the tree without bound.
    static constexpr uint32_t kMaxDepth = 24;
    // wall times of this many recent frames are kept for the report
    static constexpr uint32_t kFrameHistory = 120;
    // Weight of the newest frame in the smoothed view of a node. It is small on
    // purpose: a scene that animates moves every number from one frame to the
    // next, and a panel that shows a single frame of it is unreadable.
    static constexpr double kRecentWeight = 0.05;

    // The counter the profiler reads; cheap on purpose, it runs twice per sample.
    static Ticks Now();
    static double ToMilliseconds(Ticks ticks);

    // Which window of sampling is being read. The frame being collected is not
    // something to report on: it is only whole once EndFrame has closed it, and
    // a HUD is drawn in the middle of a frame.
    enum class Period {
        kRun,   // everything since the profiler started
        kFrame, // the frame being collected right now
        kLast,  // the frame that ended last, which is what a report shows
    };

    // What one node measured over one window.
    struct Span {
        uint32_t calls = 0;
        Ticks total = 0;
        // The span of the node with the spans of its children taken out. It
        // still carries the sampling cost of those children, which is what
        // SelfTimeMs takes off.
        Ticks self = 0;
        Ticks min = kMaxTicks;
        Ticks max = 0;

        double total_ms() const { return ToMilliseconds(total); }
        double self_ms() const { return ToMilliseconds(self); }
        double min_ms() const { return calls > 0 ? ToMilliseconds(min) : 0.0; }
        double max_ms() const { return calls > 0 ? ToMilliseconds(max) : 0.0; }
        double avg_ms() const { return calls > 0 ? total_ms() / calls : 0.0; }

        void Reset();
    };

    // What a node cost on average over the recent frames. A panel shows this
    // rather than a single frame, which jitters far too much to read.
    struct Average {
        double calls = 0.0;
        double total_ms = 0.0;
        // before the sampling correction, which AverageSelfMs takes off
        double self_ms = 0.0;
    };

    class Node : private Uncopyable {
    public:
        Node(const char* name, Node* parent, uint32_t depth);

        // WARNINGS:
        // The string used is assumed to be a static string;
        // pointer compares are used throughout the profiling code for efficiency.
        const char* name() const { return name_; }
        Node* parent() const { return parent_; }
        uint32_t depth() const { return depth_; }

        bool IsRoot() const { return parent_ == nullptr; }

        const Span& span(Period period) const {
            return period == Period::kFrame ? frame_ : (period == Period::kLast ? last_ : run_);
        }

        const Average& average() const { return average_; }
        // smoothed cost of one frame, over the recent frames
        double recent_time() const { return average_.total_ms; }

        Node* FindChild(const char* name);
        const std::vector<Node*>& children() const { return children_; }

        void BeginSample(Ticks now);
        void EndSample(Ticks now);

    private:
        friend class Profiler;

        void ResetFrame();
        void CloseFrame();

        const char* name_;
        Node* parent_;
        uint32_t depth_;

        std::vector<Node*> children_;
        // The child asked for last, which is the one the next sample of the same
        // call site asks for again - a pass binding many materials, say. One
        // compare instead of walking the children.
        Node* last_child_ = nullptr;

        Ticks begin_ = 0;

        Span run_;
        Span frame_;
        // the frame that ended last, kept whole so that whatever draws between
        // two frames - the HUD - reads a complete one
        Span last_;

        Average average_;
    };

    Profiler();

    // Sampling can be switched off for a run, which leaves a guard costing a
    // call and a branch. It cannot make the samples themselves free, and
    // switching it while samples are open unbalances them, so do it between
    // frames.
    bool enabled() const { return enabled_; }
    void SetEnabled(bool enabled) { enabled_ = enabled; }

    // Manual sampling, for a span that is not a scope. The engine samples with
    // PerfSample/PerfGuard instead.
    void BeginSample(const char* name);
    void EndSample();

    // Statistics are reported per frame, so a frame has to be marked: call
    // BeginFrame before the work of a frame and EndFrame after it. wall_seconds
    // is how long the frame took for the loop that ran it, and it is where the
    // FPS of the report comes from; 0 falls back to what the frame span timed.
    void BeginFrame();
    void EndFrame(double wall_seconds = 0.0);

    Node* root() { return &root_; }
    // The node that spans a frame; the children of it are the top of a per
    // frame report.
    Node* frame_node() { return frame_node_; }

    // The self time of a node over a window, with the sampling that came with
    // its children taken off, and never below zero. This is the number a report
    // or a panel points at.
    double SelfTimeMs(const Node& node, Period period) const;
    // The same over the smoothed view.
    double AverageSelfMs(const Node& node) const;

    // What one Begin/End pair costs, measured at startup. It is what SelfTimeMs
    // takes off, so a sample no longer pays for being measured.
    double sample_overhead_ms() const;
    double sample_overhead_ns() const;
    // The smallest step the clock takes, which is the resolution of every number
    // the profiler reports: a sample shorter than this measures as no time at
    // all. One sample costs less than the step on a fast machine, which is why
    // the overhead above is a fraction of a tick and not a whole one.
    double clock_step_ns() const;

    uint32_t frame_count() const { return frame_count_; }
    // milliseconds of the frame that ended last, as the loop timed it
    double last_frame_ms() const { return last_frame_ms_; }
    // milliseconds, averaged over the recent frames
    double average_frame_ms() const;
    double frame_rate() const;

    // The GPU time of the frame that ended last. The renderer knows it (see
    // PerfStats) and hands it over once per frame; the profiler samples the CPU
    // only and cannot measure the GPU itself.
    void SetFrameGpuTime(double ms) { frame_gpu_ms_ = ms; }
    double frame_gpu_ms() const { return frame_gpu_ms_; }
    // What the frame that ended last spent waiting on the GPU. The waits are
    // sampled like everything else and are found by their names - they all start
    // with "WaitFor" (see the D3D12 backend) - which keeps this free of any
    // knowledge of what is being waited for. frame_ms() minus this is the CPU
    // work of the frame, the part that is left when the GPU is not in the way.
    double FrameWaitMs() const;

    // The frame that ended last, then everything sampled since the start. This
    // is what a program prints once on the way out.
    void PrintAll();
    // Only the frame that ended last.
    void PrintFrame();
    // Only the whole run.
    void PrintLifetime();

private:
    Node* CreateChild(Node* parent, const char* name);
    // Closes whatever a frame left open, so a sample lost to an early return or
    // an exception is not charged to the next frame.
    void CloseOpenSamples();
    // Measures the cost of one Begin/End pair. Runs from the constructor, on a
    // node that is unlinked again before anything can read it.
    void Calibrate();

    // takes the sampling that came with `nested_calls` child samples off a self
    // time, and never goes below zero
    double CorrectedSelfMs(double self_ms, double nested_calls) const;

    void PrintFrameNode(const Node& node, double frame_ms, int spacing);
    void PrintLifetimeNode(const Node& node, int spacing);

    void Printf(const char* fmt, ...);

    Node root_;
    Node* current_node_;
    Node* frame_node_ = nullptr;
    // owns every node but the root, and hands out the stable addresses the tree
    // is built from
    std::vector<std::unique_ptr<Node>> nodes_;

    bool enabled_ = true;
    // samples that closed more often than they opened, and samples dropped for
    // being too deep; both are zero unless something is wrong
    uint32_t unbalanced_samples_ = 0;
    uint32_t skipped_samples_ = 0;
    // guards the depth clamp, which is not a stack of its own: a sample that is
    // too deep is only ever nested inside another one that is
    uint32_t clamped_samples_ = 0;

    // What one Begin/End pair costs, in ticks. It is a fraction rather than a
    // whole number of ticks, so it cannot be an integer.
    double overhead_ticks_ = 0.0;
    Ticks clock_step_ticks_ = 0;

    uint32_t frame_count_ = 0;
    double last_frame_ms_ = 0.0;
    double frame_gpu_ms_ = 0.0;
    double frame_wall_history_[kFrameHistory] = {};
    uint32_t frame_wall_count_ = 0;
    uint32_t frame_wall_head_ = 0;
};

class PerfGuard : private Uncopyable {
public:
    explicit PerfGuard(const char* name) {
        Profiler::Instance()->BeginSample(name);
    }

    ~PerfGuard() {
        Profiler::Instance()->EndSample();
    }
};

}

#define GLACIER_PERF_JOIN2(a, b) a##b
#define GLACIER_PERF_JOIN(a, b) GLACIER_PERF_JOIN2(a, b)

// WARNINGS:
// The string used is assumed to be a static string;
// pointer compares are used throughout the profiling code for efficiency.
// __COUNTER__ rather than __LINE__, so that two samples on one line, or two in
// one scope, do not name the same guard.
#define PerfSample(name) glacier::PerfGuard GLACIER_PERF_JOIN(__perf_guard_, __COUNTER__)(name)
