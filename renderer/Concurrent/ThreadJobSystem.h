#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <mutex>
#include "Common/Uncopyable.h"
#include "Common/Bitutil.h"
#include "Spinlock.h"
#include "Common/Singleton.h"
#include "RingBuffer.h"

namespace glacier {
namespace concurrent {

class ThreadJobSystem : public Singleton<ThreadJobSystem> {
public:
    struct JobArgs
    {
        uint32_t jobIndex;	    // job index relative to dispatch (like SV_DispatchThreadID in HLSL)
        uint32_t groupID;    // group index relative to dispatch (like SV_GroupID in HLSL)
        uint32_t groupIndex;    // job index relative to group (like SV_GroupIndex in HLSL)
        bool isFirstJobInGroup;    // is the current job the first one in the group?
        bool isLastJobInGroup;	    // is the current job the last one in the group?
    };

    struct Job
    {
        std::function<void(JobArgs)> task;
        uint32_t groupID;
        uint32_t groupJobOffset;
        uint32_t groupJobEnd;
    };

    void Initialize(uint32_t thread_count);

    bool IsBusy();
    void Wait();

    uint32_t GetThreadCount() { return numThreads; }

    void Schedule(const std::function<void(JobArgs)>& task);
    void Schedule(uint32_t jobCount, uint32_t groupSize, const std::function<void(JobArgs)>& task);

private:
    bool Work();

    uint32_t numThreads;
    uint32_t numCores;
    
    RingBuffer<Job, 256> jobQueue;

    std::atomic<uint32_t> counter{ ATOMIC_FLAG_INIT };
    std::atomic_bool alive{ true };
    std::condition_variable wakeCondition;
    std::mutex wakeMutex;
};

}
}
