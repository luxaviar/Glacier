#pragma once

#include <atomic>
#include "Common/Uncopyable.h"

namespace glacier {

struct SpinLock : private Uncopyable {
    SpinLock(std::atomic_flag& flag_) : flag(flag_) {
        while (flag.test_and_set(std::memory_order_acquire));
    }

    ~SpinLock() {
        flag.clear(std::memory_order_release);
    }

    std::atomic_flag& flag;
};

}
