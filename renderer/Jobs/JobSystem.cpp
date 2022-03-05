#include "JobSystem.h"
#include "Exception/Exception.h"
#include "Inspect/Timer.h"

namespace glacier {
namespace jobs {

static double GetThreadTime() {
    DWORD tid = GetCurrentThreadId();
    HANDLE thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, tid);

    FILETIME creation, exit, kern, user;
    GetThreadTimes(thread, &creation, &exit, &kern, &user);
    uint64_t t1 = (uint64_t(kern.dwHighDateTime) << 32) | kern.dwLowDateTime;
    uint64_t t2 = (uint64_t(user.dwHighDateTime) << 32) | user.dwLowDateTime;
    return (t1 + t2) / 1e7;
}

void JobSystem::Initialize(uint32_t thread_count) {
    job_fiber_cookie_ = FlsAlloc(NULL);
    ThrowAssert(job_fiber_cookie_, "FlsAlloc(...)");

    thread_pool_.Initialize(thread_count);
    thread_count = thread_pool_.GetThreadCount();

    fiber_pool_.Create(JobFiberPool::kMaxJobFiber - thread_count,
        [this](void* param) {
            JobFiber* fiber = (JobFiber*)param;
            FiberLoop(fiber);
            return 0;
        }
    );

    for (uint32_t i = 0; i < thread_count; ++i) {
        thread_pool_.Schedule([this]() {
            ThreadLoop();
        });
    }
}

void JobSystem::Shutdown() {
    Exit();

    thread_pool_.JoinAll();
    fiber_pool_.Release();

    FlsFree(job_fiber_cookie_);
}

bool JobSystem::IsComplete(const JobHandle& handle) {
    if (handle.index == JobHandle::kInvalidIndex) return true;

    return job_queue_.IsComplete(handle.index, handle.version);
}

uint32_t JobSystem::DispatchGroupCount(uint32_t job_count, uint32_t batch_size) {
    return (job_count + batch_size - 1) / batch_size;
}

JobHandle JobSystem::Schedule(JobDelegate&& task, JobPriority pri) {
    JobHandle handle;
    handle.priority = pri;

    job_queue_.Push([&task, &handle, pri](Job& job, uint32_t index) {
        job.task = std::move(task);
        job.priority = pri;

        handle.index = index;
        handle.version = job.version.load(std::memory_order_relaxed);
    });

    return handle;
}

JobHandle JobSystem::Schedule(JobDelegate&& task, const JobHandle& wait_handle, JobPriority pri) {
    JobHandle handle;
    handle.priority = pri;

    if ((int)pri < (int)wait_handle.priority) {
        handle.priority = wait_handle.priority;
    }

    job_queue_.Push([&task, &handle, &wait_handle](Job& job, uint32_t index) {
        job.task = std::move(task);
        job.priority = handle.priority;
        job.wait_handle = wait_handle;

        handle.index = index;
        handle.version = job.version.load(std::memory_order_relaxed);
    });

    return handle;
}

JobHandle JobSystem::Schedule(const ParallelJobDelegate& task, uint32_t job_count,
    uint32_t batch_size, JobPriority pri)
{
    JobHandle handle;
    handle.priority = pri;

    uint32_t dispatch_count = 0;
    if (batch_size == 0) {
        dispatch_count = thread_pool_.GetThreadCount();
    }
    else {
        dispatch_count = DispatchGroupCount(job_count, batch_size);
    }

    if (dispatch_count == 0) {
        handle.index = JobHandle::kInvalidIndex;
        return handle;
    }

    Job* parent = job_queue_.Push([&handle, dispatch_count, pri](Job& job, uint32_t index) {
        job.dependancy_counter.store(dispatch_count, std::memory_order_relaxed);
        job.priority = pri;

        handle.index = index;
        handle.version = job.version.load(std::memory_order_relaxed);
    });

    assert(parent);

    for (uint32_t i = 0; i < dispatch_count; ++i) {
        job_queue_.Push([&task, parent, pri, job_count, batch_size, i](Job& job, uint32_t index) {
            job.parallel_task = task;
            job.priority = pri;
            job.parent = parent;
            job.batch_id = i;
            job.batch_begin = i * batch_size;
            job.batch_end = std::min(job.batch_begin + batch_size, job_count);
        });
    }

    return handle;
}

JobHandle JobSystem::Schedule(const ParallelJobDelegate& task, uint32_t job_count, uint32_t batch_size,
    const JobHandle& wait_handle, JobPriority pri)
{
    JobHandle handle;
    uint32_t dispatch_count = 0;

    if (batch_size == 0) {
        dispatch_count = thread_pool_.GetThreadCount();
    }
    else {
        dispatch_count = DispatchGroupCount(job_count, batch_size);
    }

    if (dispatch_count == 0) {
        handle.index = JobHandle::kInvalidIndex;
        return handle;
    }

    if ((int)pri < (int)wait_handle.priority) {
        pri = wait_handle.priority;
    }

    handle.priority = pri;

    Job* parent = job_queue_.Push([&handle, dispatch_count](Job& job, uint32_t index) {
        job.dependancy_counter.store(dispatch_count, std::memory_order_relaxed);
        job.priority = handle.priority;

        handle.index = index;
        handle.version = job.version.load(std::memory_order_relaxed);
    });

    assert(parent);

    for (uint32_t i = 0; i < dispatch_count; ++i) {
        auto batch_begin = i * batch_size;
        auto batch_end = std::min(batch_begin + batch_size, job_count);
        job_queue_.Push([&task, parent, &wait_handle, pri, batch_begin, batch_end, i](Job& job, uint32_t index) {
            job.parallel_task = task;
            job.priority = pri;
            job.wait_handle = wait_handle;
            job.parent = parent;
            job.batch_id = i;
            job.batch_begin = batch_begin;
            job.batch_end = batch_end;
        });
    }

    return handle;
}

void JobSystem::Exit() noexcept {
    keep_running_.store(false, std::memory_order_relaxed);
}

void JobSystem::WaitUntilFinish() {
    std::exception_ptr exp;

    while (true) {
        if (keep_running_.load(std::memory_order_relaxed) && job_queue_.GetJobCount() > 0) {
            if (thread_pool_.RaiseExcpetion(exp)) {
                break;
            }
        }
        else {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    Shutdown();

    if (exp) {
        std::rethrow_exception(exp);
    }
}

void JobSystem::YieldJob() {
    JobFiber* self_fiber = (JobFiber*)FlsGetValue(job_fiber_cookie_);
    if (self_fiber) {
        JobFiber* next_fiber = PullActiveJob();

        if (next_fiber) {
            assert(next_fiber->job);
            if (!next_fiber->job->HasDependency()) {
                fiber_pool_.Suspend(self_fiber);
                SwitchToFiber(next_fiber->fiber.GetAddress());
                return;
            }
            fiber_pool_.Suspend(next_fiber);
        }
        else {
            Job* job = job_queue_.TakeOne();
            if (job) {
                if (!job->HasDependency()) {
                    next_fiber = fiber_pool_.AllocFiber(job);
                    if (next_fiber) {
                        fiber_pool_.Suspend(self_fiber);
                        SwitchToFiber(next_fiber->fiber.GetAddress());
                        return;
                    }
                }
                job_queue_.Suspend(job);
            }
        }
    }
    std::this_thread::yield();
}

void JobSystem::ThreadLoop() {
    ThrowIfFailed(CoInitialize(NULL), "CoInitialize(...)");

    double start_thread = GetThreadTime();
    auto start = std::chrono::high_resolution_clock::now();

    auto thread_fiber = ConvertThreadToFiber(NULL);
    auto ptr = fiber_pool_.Create(thread_fiber);

    FiberLoop(ptr);

    ConvertFiberToThread();

    double end_thread = GetThreadTime();
    double duration = (std::chrono::high_resolution_clock::now() - start).count() / 1e9;
    double cpuUsage = 100 * (end_thread - start_thread) / duration;
    
    char buf[1024];
    snprintf(buf, 1024, "cpu usage: %.1lf\n", cpuUsage);
    
    OutputDebugStringA(buf);

    CoUninitialize();
}

JobFiber* JobSystem::PullActiveJob() {
    JobFiber* fiber = fiber_pool_.TakeOne();
    return fiber;
}

void JobSystem::FiberLoop(JobFiber* self_fiber) {
    FlsSetValue(job_fiber_cookie_, self_fiber);
    
    Timer timer;
    while (keep_running_.load(std::memory_order_relaxed)) {
        auto self_job = self_fiber->job;
        if (self_job) {
            if (self_job->task) {
                self_job->task();
            }
            else if (self_job->parallel_task) {
                for (int i = self_job->batch_begin; i < self_job->batch_end; ++i) {
                    self_job->parallel_task(i);
                }
            }

            //TODO: resume parent job immediately
            if (self_job->parent) { //parent job shoudn't be binded with fiber
                self_job->parent->dependancy_counter.fetch_sub(1, std::memory_order_relaxed);
            }

            self_fiber->job = nullptr;
            job_queue_.Free(self_job);
            timer.Mark();

            continue;
        }
        else {
            JobFiber* next_fiber = PullActiveJob();

            if (next_fiber) {
                assert(next_fiber->job);
                if (!next_fiber->job->HasDependency()) {
                    fiber_pool_.FreeFiber(self_fiber);
                    SwitchToFiber(next_fiber->fiber.GetAddress());
                    timer.Mark();
                    continue;
                }
                
                fiber_pool_.Suspend(next_fiber);
            }
            else {
                Job* job = job_queue_.TakeOne();
                if (job) {
                    if (!job->HasDependency()) {
                        self_fiber->job = job;
                        continue;
                    }
                    
                    job_queue_.Suspend(job);
                }
            }
        }

        if (timer.DeltaTime() > 0.015f) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            timer.Mark();
        }
    }
}

}
}
