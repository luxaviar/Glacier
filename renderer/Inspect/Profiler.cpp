#include "Profiler.h"
#include <windows.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include "Common/Log.h"

namespace glacier {

namespace {
// one QPC tick in milliseconds. The frequency never changes, so it is read once
// and kept; the conversion only happens when a report is written.
double MillisecondsPerTick() {
    static const double kScale = [] {
        LARGE_INTEGER frequency;
        ::QueryPerformanceFrequency(&frequency);
        return 1000.0 / (double)frequency.QuadPart;
    }();

    return kScale;
}

const char kFrameName[] = "Frame";
}

Profiler::Ticks Profiler::Now() {
    LARGE_INTEGER counter;
    ::QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

double Profiler::ToMilliseconds(Ticks ticks) {
    return (double)ticks * MillisecondsPerTick();
}

double Profiler::sample_overhead_ms() const {
    return overhead_ticks_ * MillisecondsPerTick();
}

double Profiler::sample_overhead_ns() const {
    return overhead_ticks_ * MillisecondsPerTick() * 1000.0 * 1000.0;
}

double Profiler::clock_step_ns() const {
    return (double)clock_step_ticks_ * MillisecondsPerTick() * 1000.0 * 1000.0;
}

void Profiler::Span::Reset() {
    calls = 0;
    total = 0;
    self = 0;
    min = kMaxTicks;
    max = 0;
}

Profiler::Node::Node(const char* name, Node* parent, uint32_t depth) :
    name_(name),
    parent_(parent),
    depth_(depth)
{
}

Profiler::Node* Profiler::Node::FindChild(const char* name) {
    // the child of the previous sample of this call site, which is the one a
    // loop of draws asks for over and over
    if (last_child_ != nullptr && last_child_->name_ == name) {
        return last_child_;
    }

    for (auto* child : children_) {
        if (child->name_ == name) {
            last_child_ = child;
            return child;
        }
    }

    return nullptr;
}

void Profiler::Node::BeginSample(Ticks now) {
    ++run_.calls;
    ++frame_.calls;
    begin_ = now;
}

void Profiler::Node::EndSample(Ticks now) {
    const Ticks diff = now - begin_;

    run_.total += diff;
    run_.self += diff;
    frame_.total += diff;
    frame_.self += diff;

    if (diff > run_.max) {
        run_.max = diff;
    }

    if (diff < run_.min) {
        run_.min = diff;
    }

    if (diff > frame_.max) {
        frame_.max = diff;
    }

    if (diff < frame_.min) {
        frame_.min = diff;
    }

    if (parent_ != nullptr) {
        // the span of this node is time its parent did not spend on its own
        // work, so it comes off the parent's self time
        parent_->run_.self -= diff;
        parent_->frame_.self -= diff;
    }
}

void Profiler::Node::ResetFrame() {
    frame_.Reset();
}

void Profiler::Node::CloseFrame() {
    // the frame is whole now, so it is what a report and the panel read
    last_ = frame_;

    // A node that did not run in this frame decays towards zero as well, so a
    // one off spike does not stay in the average; a node that ran in it pulls
    // the average towards what it cost.
    const double weight = kRecentWeight;
    average_.calls += ((double)frame_.calls - average_.calls) * weight;
    average_.total_ms += (ToMilliseconds(frame_.total) - average_.total_ms) * weight;
    average_.self_ms += (ToMilliseconds(frame_.self) - average_.self_ms) * weight;
}

Profiler::Profiler() : root_("Root", nullptr, 0) {
    current_node_ = &root_;
    nodes_.reserve(64);

    Calibrate();
}

Profiler::Node* Profiler::CreateChild(Node* parent, const char* name) {
    nodes_.push_back(std::make_unique<Node>(name, parent, parent->depth_ + 1));

    auto* node = nodes_.back().get();
    parent->children_.push_back(node);
    parent->last_child_ = node;

    return node;
}

void Profiler::Calibrate() {
    // The clock steps in whole ticks, and every number the profiler reports is a
    // whole number of them: a sample shorter than a step measures as no time at
    // all. Knowing the step is what says how small a reading is still a reading.
    Ticks step = kMaxTicks;
    for (int i = 0; i < 4096; ++i) {
        const Ticks a = Now();
        const Ticks b = Now();
        if (b != a) {
            step = std::min(step, b - a);
        }
    }

    clock_step_ticks_ = step == kMaxTicks ? 0 : step;

    // A sample pays for the profiler: two reads of the clock, the walk to the
    // node, and the bookkeeping. That cost lands inside whichever interval is
    // open at the time, so a report that wants to be believed has to know how
    // big it is - and it is a very different number in Debug and in Release,
    // which is why it is measured rather than assumed.
    //
    // The measurement runs the real sampling path, on a node the constructor
    // unlinks again before anything can read the tree.
    constexpr int kRounds = 16;
    constexpr int kTrials = 256;

    Ticks best = kMaxTicks;
    for (int round = 0; round < kRounds; ++round) {
        const Ticks round_begin = Now();
        for (int i = 0; i < kTrials; ++i) {
            BeginSample("calibration");
            EndSample();
        }
        const Ticks round_end = Now();

        // the smallest round is the one the scheduler did not interrupt
        best = std::min(best, round_end - round_begin);
    }

    // One sample costs less than a single tick, so a round is kept whole and
    // divided once, in floating point. Dividing a round by its trial count first
    // truncates every round to zero, and the smallest of them is then zero too.
    overhead_ticks_ = (double)best / (double)kTrials;

    // The calibration node is the last child of the root and the last node
    // created, so taking both back leaves the tree as it was.
    if (!root_.children_.empty()) {
        root_.children_.pop_back();
        root_.last_child_ = root_.children_.empty() ? nullptr : root_.children_.back();
        nodes_.pop_back();
    }

    root_.run_.Reset();
    root_.frame_.Reset();
    root_.last_.Reset();

    current_node_ = &root_;
    frame_node_ = nullptr;
    skipped_samples_ = 0;
    clamped_samples_ = 0;
    unbalanced_samples_ = 0;
}

void Profiler::BeginSample(const char* name) {
    if (!enabled_) {
        return;
    }

    auto* node = current_node_->FindChild(name);
    if (node == nullptr) {
        // A sample nested past the limit is timed into the node already on the
        // stack: a runaway recursion then costs a counter, not the tree.
        if (current_node_->depth_ >= kMaxDepth) {
            ++skipped_samples_;
            ++clamped_samples_;
            return;
        }

        node = CreateChild(current_node_, name);
    }

    current_node_ = node;
    node->BeginSample(Now());
}

void Profiler::EndSample() {
    if (!enabled_) {
        return;
    }

    // a sample that was dropped for being too deep closes nothing
    if (clamped_samples_ > 0) {
        --clamped_samples_;
        return;
    }

    assert(current_node_ != &root_);
    if (current_node_ == &root_) {
        ++unbalanced_samples_;
        return;
    }

    current_node_->EndSample(Now());
    current_node_ = current_node_->parent_;
}

void Profiler::CloseOpenSamples() {
    while (current_node_ != &root_) {
        current_node_->EndSample(Now());
        current_node_ = current_node_->parent_;
        ++unbalanced_samples_;
    }

    // whatever was clamped belonged to the frame being closed
    clamped_samples_ = 0;
}

void Profiler::BeginFrame() {
    CloseOpenSamples();

    // the frame numbers of the nodes are cleared here rather than when the
    // frame ends, so a report written between two frames still describes the
    // frame that just finished
    for (auto& node : nodes_) {
        node->ResetFrame();
    }

    ++frame_count_;

    if (!enabled_) {
        frame_node_ = nullptr;
        return;
    }

    BeginSample(kFrameName);
    frame_node_ = current_node_;
}

void Profiler::EndFrame(double wall_seconds) {
    if (enabled_) {
        // a frame that an early return or an exception left open is closed
        // here, so its time is not charged to the frame that follows it
        while (current_node_ != &root_ && current_node_ != frame_node_) {
            current_node_->EndSample(Now());
            current_node_ = current_node_->parent_;
            ++unbalanced_samples_;
        }

        if (current_node_ == frame_node_) {
            EndSample();
        }

        CloseOpenSamples();

        for (auto& node : nodes_) {
            node->CloseFrame();
        }
    }

    if (wall_seconds > 0.0) {
        last_frame_ms_ = wall_seconds * 1000.0;
    }
    else {
        last_frame_ms_ = frame_node_ != nullptr ? frame_node_->span(Period::kLast).total_ms() : 0.0;
    }

    frame_wall_history_[frame_wall_head_] = last_frame_ms_;
    frame_wall_head_ = (frame_wall_head_ + 1) % kFrameHistory;
    if (frame_wall_count_ < kFrameHistory) {
        ++frame_wall_count_;
    }
}

double Profiler::CorrectedSelfMs(double self_ms, double nested_calls) const {
    // The self time of a node is its span with its children's spans taken out,
    // which leaves the sampling cost that came with those children: their
    // Begin/End pair ran inside the span as well. One overhead per nested
    // sample takes it back off. The node's own pair ran in its parent's span,
    // so it is the parent that pays for it.
    const double self = self_ms - nested_calls * sample_overhead_ms();
    return self > 0.0 ? self : 0.0;
}

double Profiler::SelfTimeMs(const Node& node, Period period) const {
    double nested_calls = 0.0;
    for (auto* child : node.children()) {
        nested_calls += (double)child->span(period).calls;
    }

    return CorrectedSelfMs(node.span(period).self_ms(), nested_calls);
}

double Profiler::AverageSelfMs(const Node& node) const {
    double nested_calls = 0.0;
    for (auto* child : node.children()) {
        nested_calls += child->average().calls;
    }

    return CorrectedSelfMs(node.average().self_ms, nested_calls);
}

double Profiler::average_frame_ms() const {
    if (frame_wall_count_ == 0) {
        return 0.0;
    }

    double total = 0.0;
    for (uint32_t i = 0; i < frame_wall_count_; ++i) {
        total += frame_wall_history_[i];
    }

    return total / frame_wall_count_;
}

double Profiler::frame_rate() const {
    const auto average = average_frame_ms();
    return average > 0.0 ? 1000.0 / average : 0.0;
}

double Profiler::FrameWaitMs() const {
    double total = 0.0;

    // every wait of the frame is named "WaitFor...", so summing the nodes that
    // say so gives what the GPU held the frame up by; a name is a static string
    // and the compare is on the text, not on the pointer, because the same wait
    // can appear under two parents (the frame flush and the backbuffer)
    for (auto& node : nodes_) {
        if (strncmp(node->name(), "WaitFor", 7) == 0) {
            total += node->span(Period::kLast).total_ms();
        }
    }

    return total;
}

void Profiler::PrintAll() {
    PrintFrame();
    PrintLifetime();
}

void Profiler::PrintFrame() {
    const auto* span = frame_node_ != nullptr ? &frame_node_->span(Period::kLast) : nullptr;
    const double frame_ms = span != nullptr ? span->total_ms() : 0.0;
    const double recent_ms = frame_node_ != nullptr ? frame_node_->recent_time() : 0.0;

    double min_wall = 0.0;
    double max_wall = 0.0;
    if (frame_wall_count_ > 0) {
        min_wall = max_wall = frame_wall_history_[0];
        for (uint32_t i = 1; i < frame_wall_count_; ++i) {
            min_wall = std::min(min_wall, frame_wall_history_[i]);
            max_wall = std::max(max_wall, frame_wall_history_[i]);
        }
    }

    Printf("--- profiler: the frame that ended last (%u frames so far) ---", frame_count_);
    Printf("profiled %.3f ms, smoothed %.3f ms, the frame that ended last", frame_ms, recent_ms);
    Printf("wall over the last %u frames: avg %.3f ms (%.1f fps)   min %.3f ms   max %.3f ms",
        frame_wall_count_, average_frame_ms(), frame_rate(), min_wall, max_wall);

    // the three numbers a frame can be read from: what the CPU did, what the GPU
    // did, and what the frame waited for (a serialized frame is cpu + wait, with
    // the wait being the GPU of the frame before)
    const double wait_ms = FrameWaitMs();
    const double cpu_ms = frame_ms > wait_ms ? frame_ms - wait_ms : 0.0;
    Printf("cpu %.3f ms, gpu %.3f ms, frame %.3f ms, wait %.3f ms, gpu %.1f%% of the frame",
        cpu_ms, frame_gpu_ms_, frame_ms, wait_ms, frame_ms > 0.0 ? frame_gpu_ms_ / frame_ms * 100.0 : 0.0);
    Printf("the wait is what held the frame up: a flush, a backbuffer, or the frame contexts of the frames in flight");
    Printf("while the cpu runs ahead of the gpu - the usual case - the loop costs what wall says, not cpu + wait");

    Printf("wall is the whole loop iteration, of which the profiled span is a part");
    Printf("sampling costs about %.0f ns per sample, and the clock steps every %.0f ns;",
        sample_overhead_ns(), clock_step_ns());
    Printf("the first is taken off the self column, the second is how small a reading is still a reading");

    if (skipped_samples_ > 0 || unbalanced_samples_ > 0) {
        Printf("dropped %u samples past the depth limit, closed %u samples left open",
            skipped_samples_, unbalanced_samples_);
    }

    Printf("%-40s %9s %9s %7s %8s %8s %9s %8s",
        "node", "total", "self", "calls", "avg", "max", "smoothed", "%frame");

    for (auto* child : root_.children()) {
        PrintFrameNode(*child, frame_ms, 0);
    }
}

void Profiler::PrintFrameNode(const Node& node, double frame_ms, int spacing) {
    const auto& children = node.children();
    const auto& span = node.span(Period::kLast);

    if (span.calls == 0) {
        // the node did not run in this frame, but one of its children may have
        for (auto* child : children) {
            PrintFrameNode(*child, frame_ms, spacing);
        }

        return;
    }

    char label[160];
    snprintf(label, sizeof(label), "%*s%s", spacing, "", node.name());

    Printf("%-40s %9.3f %9.3f %7u %8.3f %8.3f %9.3f %7.1f%%",
        label, span.total_ms(), SelfTimeMs(node, Period::kLast), span.calls,
        span.avg_ms(), span.max_ms(), node.recent_time(),
        frame_ms > 0.0 ? span.total_ms() / frame_ms * 100.0 : 0.0);

    for (auto* child : children) {
        PrintFrameNode(*child, frame_ms, spacing + 2);
    }
}

void Profiler::PrintLifetime() {
    Printf("--- profiler: whole run, %u frames ---", frame_count_);
    Printf("%-40s %10s %10s %8s %9s %9s %9s",
        "node", "total", "self", "calls", "avg", "max", "min");

    for (auto* child : root_.children()) {
        PrintLifetimeNode(*child, 0);
    }
}

void Profiler::PrintLifetimeNode(const Node& node, int spacing) {
    const auto& children = node.children();
    const auto& span = node.span(Period::kRun);

    char label[160];
    snprintf(label, sizeof(label), "%*s%s", spacing, "", node.name());

    Printf("%-40s %10.3f %10.3f %8u %9.3f %9.3f %9.3f",
        label, span.total_ms(), SelfTimeMs(node, Period::kRun), span.calls,
        span.avg_ms(), span.max_ms(), span.min_ms());

    for (auto* child : children) {
        PrintLifetimeNode(*child, spacing + 2);
    }
}

void Profiler::Printf(const char* fmt, ...) {
    char buf[1024];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 1024, fmt, args);
    va_end(args);

    LOG_LOG("{}", buf);
}

}
