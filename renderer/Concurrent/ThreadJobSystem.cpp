#pragma once

#include "ThreadJobSystem.h"
#include <thread>
#include <string>

namespace glacier {
namespace concurrent {

void ThreadJobSystem::Initialize(uint32_t thread_count) {
    if (numThreads > 0)
        return;
    thread_count = std::max(1u, thread_count);

    // Retrieve the number of hardware threads in this system:
    numCores = std::thread::hardware_concurrency();

    // Calculate the actual number of worker threads we want (-1 main thread):
    numThreads = std::min(thread_count, std::max(1u, numCores - 1));

    for (uint32_t threadID = 0; threadID < numThreads; ++threadID)
    {
        std::thread worker([this] {
            while (alive.load())
            {
                if (!Work())
                {
                    // no job, put thread to sleep
                    std::unique_lock<std::mutex> lock(wakeMutex);
                    wakeCondition.wait(lock);
                }
            }
        });

#ifdef _WIN32
        // Do Windows-specific thread setup:
        HANDLE handle = (HANDLE)worker.native_handle();

        // Put each thread on to dedicated core:
        DWORD_PTR affinityMask = 1ull << threadID;
        DWORD_PTR affinity_result = SetThreadAffinityMask(handle, affinityMask);
        assert(affinity_result > 0);

        //// Increase thread priority:
        BOOL priority_result = SetThreadPriority(handle, THREAD_PRIORITY_ABOVE_NORMAL);
        assert(priority_result != 0);

        // Name the thread:
        std::wstring wthreadname = L"glacier::JobSystem " + std::to_wstring(threadID);
        HRESULT hr = SetThreadDescription(handle, wthreadname.c_str());
        assert(SUCCEEDED(hr));
#elif defined(PLATFORM_LINUX)
#define handle_error_en(en, msg) \
               do { errno = en; perror(msg); } while (0)

        int ret;
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        size_t cpusetsize = sizeof(cpuset);

        CPU_SET(threadID, &cpuset);
        ret = pthread_setaffinity_np(worker.native_handle(), cpusetsize, &cpuset);
        if (ret != 0)
            handle_error_en(ret, std::string(" pthread_setaffinity_np[" + std::to_string(threadID) + ']').c_str());

        // Name the thread
        std::string thread_name = "wi::jobsystem_" + std::to_string(threadID);
        ret = pthread_setname_np(worker.native_handle(), thread_name.c_str());
        if (ret != 0)
            handle_error_en(ret, std::string(" pthread_setname_np[" + std::to_string(threadID) + ']').c_str());
#undef handle_error_en
#endif // _WIN32

        worker.detach();
    }
}

bool ThreadJobSystem::Work() {
    Job job;
    if (jobQueue.Pop(job))
    {
        JobArgs args;
        args.groupID = job.groupID;

        for (uint32_t i = job.groupJobOffset; i < job.groupJobEnd; ++i)
        {
            args.jobIndex = i;
            args.groupIndex = i - job.groupJobOffset;
            args.isFirstJobInGroup = (i == job.groupJobOffset);
            args.isLastJobInGroup = (i == job.groupJobEnd - 1);
            job.task(args);
        }

        counter.fetch_sub(1);
        return true;
    }
    return false;
}

bool ThreadJobSystem::IsBusy() {
    return counter.load() > 0;
}

void ThreadJobSystem::Wait() {
    wakeCondition.notify_all();

    // Waiting will also put the current thread to good use by working on an other job if it can:
    while (IsBusy()) { Work(); }
}

void ThreadJobSystem::Schedule(const std::function<void(JobArgs)>& task) {
    counter.fetch_add(1);

    Job job;
    job.task = task;
    job.groupID = 0;
    job.groupJobOffset = 0;
    job.groupJobEnd = 1;

    // Try to push a new job until it is pushed successfully:
    while (!jobQueue.Push(job)) { wakeCondition.notify_all(); Work(); }

    // Wake any one thread that might be sleeping:
    wakeCondition.notify_one();
}

inline uint32_t DispatchGroupCount(uint32_t jobCount, uint32_t groupSize)
{
    // Calculate the amount of job groups to dispatch (overestimate, or "ceil"):
    return (jobCount + groupSize - 1) / groupSize;
}

void ThreadJobSystem::Schedule(uint32_t jobCount, uint32_t groupSize, const std::function<void(JobArgs)>& task)
{
    if (jobCount == 0 || groupSize == 0) {
        return;
    }

    const uint32_t groupCount = DispatchGroupCount(jobCount, groupSize);

    // Context state is updated:
    counter.fetch_add(groupCount);

    Job job;
    job.task = task;

    for (uint32_t groupID = 0; groupID < groupCount; ++groupID)
    {
        // For each group, generate one real job:
        job.groupID = groupID;
        job.groupJobOffset = groupID * groupSize;
        job.groupJobEnd = std::min(job.groupJobOffset + groupSize, jobCount);

        // Try to push a new job until it is pushed successfully:
        while (!jobQueue.Push(job)) { wakeCondition.notify_all(); Work(); }
    }

    // Wake any threads that might be sleeping:
    wakeCondition.notify_all();
}

}
}
