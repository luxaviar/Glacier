#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <atomic>
#include <functional>
#include "Common/Uncopyable.h"
#include "common/bitutil.h"
#include "spinlock.h"

namespace glacier {
namespace concurrent {

#define CACHE_LINE_SIZE 64

//Single producer/Single consumer
template<typename T, size_t Capacity>
class RingBuffer : private Uncopyable {
public:
    constexpr size_t capacity() const { return Capacity; }

    bool Push(const T& v) {
        SpinLockGuard gurad(lock_);
        size_t next = (head_ + 1) % Capacity;
        if (next != tail_) {
            data[head_] = v;
            head_ = next;
            return true;
        }

        return false;
    }

    bool Push(T&& v) {
        SpinLockGuard gurad(lock_);
        size_t next = (head_ + 1) % Capacity;
        if (next != tail_) {
            data[head_] = std::move(v);
            head_ = next;
            return true;
        }

        return false;
    }

    template<typename F>
    T* Push(const F& func) {
        SpinLockGuard gurad(lock_);
        size_t next = (head_ + 1) % Capacity;
        if (next != tail_) {
            auto& v = data[head_];
            func(v);
            head_ = next;
            return &v;
        }

        return nullptr;
    }

    bool Pop(T& v) {
        SpinLockGuard gurad(lock_);
        if (tail_ != head_) {
            std::swap(v, data[tail_]);
            //v = data[tail_];
            tail_ = (tail_ + 1) % Capacity;
            return true;
        }

        return false;
    }

    template<typename F>
    void Visit(size_t i, const F& f) {
        assert(i < Capacity);
        SpinLockGuard gurad(lock_);
        f(data[i]);
    }

protected:
    std::atomic<uint32_t> head_ = 0;
    uint8_t cache_line_pad_[CACHE_LINE_SIZE - sizeof(std::atomic<uint32_t>)];
    std::atomic<uint32_t> tail_ = 0;
    
    T data[Capacity];

    SpinLock lock_;
};

}
}
