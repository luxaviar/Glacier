#include "Profiler.h"
#include <float.h>
#include <string.h>
#include <stdarg.h>
#include "Math/Util.h"


namespace glacier {

Profiler::Node::Node(const char* name, Node* parent) :
    name_(name),
    total_calls_(0),
    total_time_{0},
    min_time_{std::numeric_limits<int64_t>::max()},
    max_time_{0},
    parent_(parent)
{
}

Profiler::Node* Profiler::Node::GetChild(const char* name) {
    for (auto it = children_.begin(); it != children_.end(); ++it) {
        if (it->name_ == name) {
            return &(*it);
        }
    }

    children_.emplace_back(name, this);
    return &children_.back();
}

void Profiler::Node::BeginSample() {
    ++total_calls_;
    begin_time_ = ClockType::now();
}

void Profiler::Node::EndSample() {
    auto diff = ClockType::now() - begin_time_;
    total_time_ += diff;

    if (diff > max_time_) {
        max_time_ = diff;
    }

    if (diff < min_time_) {
        min_time_ = diff;
    }
}

double Profiler::Node::total_time() const {
    if (!IsRoot()) {
        return total_time_.count() * kNanoToMilli;
    }
    else {
        return (ClockType::now() - begin_time_).count() * kNanoToMilli;
    }
}

Profiler::Profiler() : root_("Root", nullptr) {
    current_node_ = &root_;
    current_node_->BeginSample();
    indent_.fill(0);
}

void Profiler::BeginSample(const char* name) {
    current_node_ = current_node_->GetChild(name);
    current_node_->BeginSample();
}

void Profiler::EndSample() {
    assert(current_node_ != &root_);

    current_node_->EndSample();
    current_node_ = current_node_->parent();
}

void Profiler::PrintAll() {
    PrintRecursively(root_, 0);
}

void Profiler::PrintRecursively(const Node& node, int spacing) {
    double total_sample_time = 0.0;
    double parent_time = node.total_time();
    auto& children = node.GetChildren();

    for (auto it = children.begin(); it != children.end(); ++it) {
        double sample_time = it->total_time();
        int sample_count = it->total_calls();
        total_sample_time += sample_time;

        double percent = parent_time > math::kEpsilon ? (sample_time / parent_time) * 100 : 0.0;
        Indent(spacing);
        Printf("%s (%.2f%%) :: %.3f ms  %d calls  %.3f ms(avg) %.3f ms(max) %.3f ms(min)",
            it->name(), percent,
            sample_time, sample_count,
            it->avg_time(), it->max_time(), it->min_time());
        PrintRecursively(*it, spacing + 2);
    }

    Indent(spacing);

    if (parent_time < total_sample_time) {
        Printf("something bad happened!");
    }
    if (children.size() > 0) {
        Printf("%s (%.3f %%) :: %.3f ms", "Unaccounted:",
            parent_time > math::kEpsilon ? ((parent_time - total_sample_time) / parent_time) * 100 : 0.0,
            parent_time - total_sample_time);
    }
}

void Profiler::Printf(const char* fmt, ...) {
    char buf[1024];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, 1024, fmt, args);
    va_end(args);

    OutputDebugStringA(indent_.data());
}

void Profiler::Indent(int spacing) {
    for (int i = 0; i < spacing; ++i) {
        indent_[i] = ' ';
    }

    indent_[spacing] = 0;
}

}
