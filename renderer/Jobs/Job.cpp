#include "Job.h"
#include "Exception/Exception.h"
#include "Inspect/Timer.h"
#include "JobSystem.h"
#include "Common/Log.h"

namespace glacier {
namespace jobs {

bool JobHandle::IsComplete() const {
    return JobSystem::Instance()->IsComplete(*this);
}

void JobHandle::WaitComplete() const {
    while (!JobSystem::Instance()->IsComplete(*this)) {
        JobSystem::Instance()->YieldJob();
    }
}

bool Job::HasDependency() const {
    return dependancy_counter.load(std::memory_order_relaxed) > 0 ||
        !wait_handle.IsComplete();
}

bool Job::IsComplete(uint32_t ver) const {
    return version.load(std::memory_order_relaxed) != ver;
}

JobFiberPool::JobFiberPool() {
}

void JobFiberPool::Release() {
    for (auto& entry : job_fibers_) {
        entry.fiber.Release();
    }
}

void JobFiberPool::Create(uint32_t fiber_count, const Fiber::Delegate& callback) {
    fiber_count = std::min(fiber_count, (uint32_t)job_fibers_.size() - used_slot);

    for (uint32_t i = 0; i < fiber_count; ++i) {
        auto& job_fiber = job_fibers_[used_slot++];

        job_fiber.fiber.Init(callback, &job_fiber);
        idle_fibers_.Push(&job_fiber);
    }
}

JobFiber* JobFiberPool::Create(PVOID address) {
    if (used_slot >= job_fibers_.size()) {
        return nullptr;
    }

    auto& slot = job_fibers_[used_slot++];
    slot.fiber.Init(address);
    idle_fibers_.Push(&slot);

    return &slot;
}

JobFiber* JobFiberPool::AllocFiber(Job* job) {
    JobFiber* ptr = nullptr;
    if (idle_fibers_.Pop(ptr)) {
        ptr->job = job;
        active_fibers_[(int)ptr->job->priority].Push(ptr);
    }

    assert(ptr);
    return ptr;
}

bool JobFiberPool::FreeFiber(JobFiber* fiber) {
    fiber->job = nullptr;
    return idle_fibers_.Push(fiber);
}

JobFiber* JobFiberPool::TakeOne() {
    JobFiber* fiber = nullptr;
    for (int i = 0; i < (int)JobPriority::kCount; ++i) {
        if (active_fibers_[i].Pop(fiber)) {
            break;
        }
    }

    return fiber;
}

void JobFiberPool::Suspend(JobFiber* fiber) {
    auto& buffer = active_fibers_[(int)fiber->job->priority];
    buffer.Push(fiber);
}

JobQueue::JobQueue() {

}

Job* JobQueue::TakeOne() {
    Job* job = nullptr;
    for (int i = 0; i < (int)JobPriority::kCount; ++i) {
        if (priority_queue_[i].Pop(job)) {
            break;
        }
    }

    return job;
}

bool JobQueue::IsComplete(uint32_t index, uint32_t version) {
    auto& job = job_collection_[index];
    return job.version.load(std::memory_order_relaxed) != version;
}

void JobQueue::Suspend(Job* job) {
    auto& queue = priority_queue_[(int)job->priority];
    queue.Push(job);
}

void JobQueue::Free(Job* job) {
    job->task = nullptr;
    job->parallel_task = nullptr;
    job->version.fetch_add(1, std::memory_order_relaxed);
    //job->dependancy_counter.store(0, std::memory_order_relaxed);
    job->parent = nullptr;
    job->wait_handle.index = JobHandle::kInvalidIndex;

    job_collection_.Free(job);
    job_count_.fetch_sub(1, std::memory_order_relaxed);
}

}
}
