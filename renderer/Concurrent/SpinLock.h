#pragma once

#include <atomic>
#include "Common/Uncopyable.h"

namespace glacier {
namespace concurrent {

class SpinLock : private Uncopyable {
public:
    void Lock() {
        while (!TryLock()) {};
    }

    bool TryLock() {
        return !flag_.test_and_set(std::memory_order_acquire);
    }

    void Unlock() {
        flag_.clear(std::memory_order_release);
    }

private:
    std::atomic_flag flag_ = { ATOMIC_FLAG_INIT };
};

struct SpinLockGuard : private Uncopyable {
    SpinLockGuard(SpinLock& lock) : lock(lock) {
        lock.Lock();
    }

    ~SpinLockGuard() {
        lock.Unlock();
    }

    SpinLock& lock;
};

}
}
