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

#define UNICO_CACHE_LINE_SIZE 64

//In-place Single producer/Single consumer
template<typename T>
class RingBuffer : private Uncopyable {
public:
    struct Entry {
        Entry() : flag{ ATOMIC_FLAG_INIT }, written(false), v() {}
        std::atomic_flag flag;
        bool written;
        T v;
    };

    RingBuffer(uint32_t size) :
        reader_idx_(0),
        writer_idx_(0) {
        size = size < 8 ? 8 : size;
        capacity_ = next_power_of2(size);
        assert(capacity_ > 0 && capacity_ <= 4096);
        mask_ = capacity_ - 1;

        ring_ = static_cast<Entry*>(std::malloc(capacity_ * sizeof(Entry)));
        for (size_t i = 0; i < capacity_; ++i) {
            new (&ring_[i]) Entry();
        }
    }

    ~RingBuffer() {
        for (size_t i = 0; i < capacity_; ++i) {
            ring_[i].~Entry();
        }

        std::free(ring_);
    }

    template<typename PushFunc>
    bool PushInplace(const PushFunc& func) {
        uint32_t idx = (writer_idx_.fetch_add(1, std::memory_order_relaxed) - 1) & mask_;
        Entry& en = ring_[idx];
        SpinLock lock(en.flag);
        bool overwrite = en.written;
        func(en.v);
        en.written = true;

        return overwrite;
    }

    template<typename PopFunc>
    bool PopInplace(const PopFunc& func) {
        uint32_t idx = reader_idx_.load(std::memory_order_relaxed) & mask_;
        Entry& en = ring_[idx];
        SpinLock lock(en.flag);
        if (en.written) {
            func(en.v);
            en.written = false;
            reader_idx_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        return false;
    }

    uint32_t capacity() const { return capacity_; }

private:
    std::atomic<uint32_t> reader_idx_;
    uint8_t cache_line_pad_[UNICO_CACHE_LINE_SIZE - sizeof(std::atomic<uint32_t>)];
    std::atomic<uint32_t> writer_idx_;
    uint32_t capacity_;
    uint32_t mask_;
    Entry* ring_;
};

}
