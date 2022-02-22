#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <exception>
#include "Common/Uncopyable.h"
#include "Common/Bitutil.h"
#include "Spinlock.h"
#include "RingBuffer.h"

namespace glacier {
namespace concurrent {

class ThreadPool : private Uncopyable {
public:
    using Task = std::function<void()>;

    ~ThreadPool();

    void Initialize(uint32_t thread_count);

    bool IsBusy() noexcept;
    bool IsRunning() noexcept;
    
    void WaitUntilFinish();
    void JoinAll();

    size_t GetThreadCount() { return threads_.size(); }

    void Schedule(Task&& task);

    bool RaiseExcpetion(std::exception_ptr& exp);

private:
    bool Execute();

    uint32_t num_cores_;
    std::vector<std::thread> threads_;
    RingBuffer<Task, 256> task_queue_;

    RingBuffer<std::exception_ptr, 64> thread_exceptions_;

    std::atomic<uint32_t> counter_{ ATOMIC_FLAG_INIT };
    std::atomic_bool alive_{ true };

    std::condition_variable wake_condition_;
    std::mutex wake_mutex_;
};

}
}
