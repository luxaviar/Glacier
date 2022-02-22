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
class FixedBuffer : private Uncopyable {
public:
    struct alignas(alignof(T)) Entry {
        T value;
        uint32_t index;
        int next; //free list link
        bool active = false;
        char padding[CACHE_LINE_SIZE];
    };

    FixedBuffer() {
        for (int i = 0; i < Capacity; ++i) {
            data_[i].index = i;
            data_[i].next = (i + 1) == Capacity ? -1 : (i + 1);
        }
    }

    constexpr size_t capacity() const { return Capacity; }

    T& operator[](size_t i) {
        return data_[i].value;
    }

    const T& operator[](size_t i) const {
        return data_[i].value;
    }

    uint32_t GetIndex(T* v) const {
        Entry* entry = static_cast<Entry*>(v);
        assert(entry->index < Capacity);
        assert(entry == &data_[entry->index]);

        return entry->index;
    }

    T* Alloc(const T& v) {
        SpinLockGuard gurad(lock_);
        T* v = nullptr;
        if (free_ < 0) {
            return nullptr;
        }

        auto& entry = data_[free_];
        assert(!entry.active);

        entry.value = v;
        entry.active = true;

        free_ = entry->next;

        return &entry.value;
    }

    T* Alloc(T&& v) {
        SpinLockGuard gurad(lock_);
        T* v = nullptr;
        if (free_ < 0) {
            return nullptr;
        }

        auto& entry = data_[free_];
        assert(!entry.active);

        entry.value = std::move(v);
        entry.active = true;

        free_ = entry->next;

        return &entry.value;
    }

    template<typename F>
    T* Alloc(const F& func) {
        SpinLockGuard gurad(lock_);
        T* v = nullptr;
        if (free_ < 0) {
            return nullptr;
        }

        auto& entry = data_[free_];
        assert(!entry.active);

        func(entry.value, entry.index);
        entry.active = true;

        free_ = entry.next;

        return &entry.value;
    }

    void Free(T* v) {
        Entry* entry = (Entry*)v;

        SpinLockGuard gurad(lock_);
        assert(entry->index < Capacity);
        assert(entry->active);
        assert(entry == &data_[entry->index]);

        entry->next = free_;
        entry->active = false;

        free_ = entry->index;
    }

    template<typename F>
    void Visit(size_t i, const F& f) {
        assert(i < Capacity);
        SpinLockGuard gurad(lock_);
        f(data_[i]);
    }

protected:
    int free_ = 0; //free list
    Entry data_[Capacity];
    SpinLock lock_;
};

}
}
