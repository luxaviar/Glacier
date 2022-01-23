#pragma once

#include <string>
#include <list>
#include <chrono>
#include <array>
#include "Common/Uncopyable.h"
#include "Common/Singleton.h"

namespace glacier {

class Profiler : public Singleton<Profiler> {
public:
    using ClockType = std::conditional<std::chrono::high_resolution_clock::is_steady,
        std::chrono::high_resolution_clock, std::chrono::steady_clock >::type;

    using TimePoint = ClockType::time_point;
    using TimeDuration = std::chrono::nanoseconds;

    class Node : private Uncopyable {
    public:

        constexpr static double kNanoToMilli = 1.0 / (1000 * 1000);
        // WARNINGS:
        // The string used is assumed to be a static string;
        // pointer compares are used throughout the profiling code for efficiency.
        Node(const char* name, Node* parent);

        const char* name() const { return name_; }
        Node* parent() const { return parent_; }

        int total_calls() const { return total_calls_; }
        double total_time() const;

        double min_time() const { return total_calls_ > 0 ? min_time_.count() * kNanoToMilli : 0.0; }
        double max_time() const { return total_calls_ > 0 ? max_time_.count() * kNanoToMilli : 0.0; }
        double avg_time() const { return total_calls_ > 0 ? total_time() / total_calls_ : 0.0; }

        bool IsRoot() const { return parent_ == nullptr; }

        Node* GetChild(const char* name);
        const std::list<Node>& GetChildren() const { return children_; }

        void BeginSample();
        void EndSample();

    protected:
        const char* name_;
        int total_calls_ = 0;
        TimeDuration total_time_;
        TimePoint begin_time_;

        TimeDuration min_time_;
        TimeDuration max_time_;

        Node* parent_;
        std::list<Node> children_;
    };

    Profiler();

    void BeginSample(const char* name);
    void EndSample();

    Node* GetCurrentNode() { return current_node_; }
    void PrintAll();

private:
    void PrintRecursively(const Node& node, int spacing);
    void Indent(int spacing);
    void Printf(const char* fmt, ...);

    Node root_;
    Node* current_node_;
    std::array<char, 256> indent_;
};

class PerfGuard : private Uncopyable {
public:
    PerfGuard(const char* name) : name_(name) {
        Profiler::Instance()->BeginSample(name);
    }

    ~PerfGuard() {
        Profiler::Instance()->EndSample();
    }

    const char* name_;
};

}

// WARNINGS:
// The string used is assumed to be a static string;
// pointer compares are used throughout the profiling code for efficiency.
#define PerfSample(name) glacier::PerfGuard __profile##__LINE__(name)
