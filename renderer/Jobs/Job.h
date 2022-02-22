#pragma once

#include <array>
#include "Fiber.h"
#include "Concurrent/ThreadPool.h"
#include "Common/Singleton.h"
#include "Concurrent/FixedBuffer.h"

namespace glacier {
namespace jobs {

enum class JobPriority : uint8_t {
    kCritical = 0,
    kHigh,
    kNormal,
    kLow,
    kCount,
};

struct JobArgs
{
    uint32_t jobIndex;	    // job index relative to dispatch (like SV_DispatchThreadID in HLSL)
    uint32_t groupID;    // group index relative to dispatch (like SV_GroupID in HLSL)
    uint32_t groupIndex;    // job index relative to group (like SV_GroupIndex in HLSL)
    bool isFirstJobInGroup;    // is the current job the first one in the group?
    bool isLastJobInGroup;	    // is the current job the last one in the group?
};

struct JobHandle {
    constexpr static uint32_t kInvalidIndex = std::numeric_limits<uint32_t>::max();

    uint32_t index = kInvalidIndex;
    uint32_t version;
    JobPriority priority = JobPriority::kNormal;

    bool IsComplete() const;
    void WaitComplete() const;
};

using JobDelegate = std::function<void()>;
using ParallelJobDelegate = std::function<void(uint32_t)>;

struct JobFiber;

struct Job {
    JobDelegate task;
    ParallelJobDelegate parallel_task;
    uint32_t batch_id;
    uint32_t batch_begin;
    uint32_t batch_end;
    JobPriority priority;

    std::atomic_uint version = 0;
    std::atomic_uint dependancy_counter = 0;
    Job* parent = nullptr;
    JobHandle wait_handle;

    bool HasDependency() const;
    bool IsComplete(uint32_t ver) const;
};

struct JobFiber {
    Fiber fiber;
    Job* job = nullptr;
};

class JobFiberPool {
public:
    static constexpr uint32_t kMaxJobFiber = 256;

    JobFiberPool();
    void Release();

    void Create(uint32_t fiber_count, const Fiber::Delegate& callback);
    JobFiber* Create(PVOID address);

    JobFiber* AllocFiber(Job* job);
    bool FreeFiber(JobFiber* job);

    JobFiber* TakeOne();
    void Suspend(JobFiber* fiber);

private:
    std::atomic_uint used_slot = 0;
    std::array<JobFiber, kMaxJobFiber> job_fibers_;

    concurrent::RingBuffer<JobFiber*, kMaxJobFiber> idle_fibers_;
    std::array<concurrent::RingBuffer<JobFiber*, kMaxJobFiber>, (size_t)JobPriority::kCount> active_fibers_;
};

class JobQueue {
public:
    constexpr static uint32_t kMaxJob = 2048;

    JobQueue();

    uint32_t GetJobCount() const { return job_count_.load(std::memory_order_relaxed); }

    Job* TakeOne();
    bool IsComplete(uint32_t index, uint32_t version);

    void Suspend(Job* job);
    void Free(Job* job);

    template<typename F>
    Job* Push(const F& func) {
        Job* job = job_collection_.Alloc(func);
        if (!job) {
            return nullptr;
        }
        
        job_count_.fetch_add(1, std::memory_order_relaxed);

        auto& queue = priority_queue_[(int)job->priority];
        queue.Push(job);

        return job;
    }

private:
    std::atomic_uint job_count_ = 0;
    concurrent::FixedBuffer<Job, kMaxJob> job_collection_;
    //concurrent::FixedBuffer<Job*, kMaxJob> waiting_list_;

    std::array<concurrent::RingBuffer<Job*, kMaxJob>, (size_t)JobPriority::kCount> priority_queue_;
};

}
}
