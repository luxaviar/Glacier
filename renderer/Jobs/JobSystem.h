#pragma once

#include <array>
#include "Fiber.h"
#include "Job.h"
#include "Common/Singleton.h"

namespace glacier {
namespace jobs {

class JobSystem : public Singleton<JobSystem> {
public:
    static constexpr uint32_t kMaxJobFiber = 256;

    void Initialize(uint32_t thread_count);

    bool IsComplete(const JobHandle& handle);

    JobHandle Schedule(JobDelegate&& task, JobPriority pri = JobPriority::kNormal);
    JobHandle Schedule(JobDelegate&& task, const JobHandle& wait_handle, JobPriority pri = JobPriority::kNormal);

    JobHandle Schedule(const ParallelJobDelegate& task, uint32_t job_count, uint32_t batch_size = 0,
        JobPriority pri = JobPriority::kNormal);

    JobHandle Schedule(const ParallelJobDelegate& task, uint32_t job_count, uint32_t batch_size,
        const JobHandle& wait_handle, JobPriority pri = JobPriority::kNormal);

    void WaitUntilFinish();
    void YieldJob();
    void Exit() noexcept;

protected:
    void Shutdown();

    uint32_t DispatchGroupCount(uint32_t job_count, uint32_t batch_size);
    JobFiber* PullActiveJob();

    void ThreadLoop();
    void FiberLoop(JobFiber* ptrFiber);

private:
    concurrent::ThreadPool thread_pool_;

    JobFiberPool fiber_pool_;
    JobQueue job_queue_;

    DWORD job_fiber_cookie_;
    std::atomic_bool keep_running_ = true;
};

}
}
