#pragma once

#include "ThreadPool.h"
#include <thread>
#include <string>

namespace glacier {
namespace concurrent {

ThreadPool::~ThreadPool() {
    if (!alive_.load(std::memory_order_acquire)) return;
    JoinAll();
}

void ThreadPool::Initialize(uint32_t thread_count) {
    if (!threads_.empty())
        return;

    thread_count = std::max(1u, thread_count);
    num_cores_ = std::thread::hardware_concurrency();
    uint32_t num_threads = std::min(thread_count, std::max(1u, num_cores_ - 1));
    threads_.reserve(num_threads);

    for (uint32_t thread_id = 0; thread_id < num_threads; ++thread_id) {
        threads_.emplace_back([this] {
            while (alive_.load(std::memory_order_acquire)) {
                if (!Execute()) {
                    // no job, put thread to sleep
                    std::unique_lock<std::mutex> lock(wake_mutex_);
                    wake_condition_.wait(lock);
                }
            }
        });

        auto& worker = threads_[thread_id];

#ifdef _WIN32
        // Do Windows-specific thread setup:
        HANDLE handle = (HANDLE)worker.native_handle();

        // Put each thread on to dedicated core:
        DWORD_PTR affinityMask = 1ull << thread_id;
        DWORD_PTR affinity_result = SetThreadAffinityMask(handle, affinityMask);
        assert(affinity_result > 0);

        //// Increase thread priority:
        BOOL priority_result = SetThreadPriority(handle, THREAD_PRIORITY_ABOVE_NORMAL);
        assert(priority_result != 0);

        // Name the thread:
        std::wstring wthreadname = L"glacier::ThreadPool " + std::to_wstring(thread_id);
        HRESULT hr = SetThreadDescription(handle, wthreadname.c_str());
        assert(SUCCEEDED(hr));
#elif defined(PLATFORM_LINUX)
#define handle_error_en(en, msg) \
               do { errno = en; perror(msg); } while (0)

        int ret;
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        size_t cpusetsize = sizeof(cpuset);

        CPU_SET(thread_id, &cpuset);
        ret = pthread_setaffinity_np(worker.native_handle(), cpusetsize, &cpuset);
        if (ret != 0)
            handle_error_en(ret, std::string(" pthread_setaffinity_np[" + std::to_string(thread_id) + ']').c_str());

        // Name the thread
        std::string thread_name = "glacier::ThreadPool " + std::to_string(thread_id);
        ret = pthread_setname_np(worker.native_handle(), thread_name.c_str());
        if (ret != 0)
            handle_error_en(ret, std::string(" pthread_setname_np[" + std::to_string(thread_id) + ']').c_str());
#undef handle_error_en
#endif // _WIN32

        //worker.detach();
    }
}

bool ThreadPool::Execute() {
    Task task;
    if (task_queue_.Pop(task)) {
        try {
            task();
        }
        catch (...) {
            thread_exceptions_.Push(std::current_exception());
        }

        counter_.fetch_sub(1);
        return true;
    }
    return false;
}

bool ThreadPool::IsBusy() noexcept {
    return counter_.load() >= threads_.size();
}

bool ThreadPool::IsRunning() noexcept {
    return counter_.load() > 0;
}

void ThreadPool::WaitUntilFinish() {
    wake_condition_.notify_all();

    // Waiting will also put the current thread to good use by working on an other job if it can:
    while (IsRunning()) { Execute(); }
}

void ThreadPool::JoinAll() {
    {
        std::lock_guard gurad(wake_mutex_);
        alive_.store(false, std::memory_order_release);
    }

    WaitUntilFinish();

    for (auto& th : threads_) {
        th.join();
    }

    threads_.clear();
}

void ThreadPool::Schedule(Task&& task) {
    counter_.fetch_add(1);

    // Try to push a new job until it is pushed successfully:
    while (!task_queue_.Push(std::move(task))) {
        wake_condition_.notify_all(); //do task in idle threads
        Execute(); //do task in current thread
    }

    // Wake any one thread that might be sleeping:
    wake_condition_.notify_one();
}

bool ThreadPool::RaiseExcpetion(std::exception_ptr& exp) {
    return thread_exceptions_.Pop(exp);
}

}
}
