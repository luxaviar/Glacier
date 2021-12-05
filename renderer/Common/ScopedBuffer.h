#pragma once

#include <string.h>
#include <assert.h>
#include <type_traits>
#include "Uncopyable.h"

namespace glacier {

template<typename T, size_t OPT_SIZE = 256>
struct ScopedBuffer : private Uncopyable {
    ScopedBuffer() : size(OPT_SIZE) {
        Alloc(OPT_SIZE);
    }

    explicit ScopedBuffer(size_t len) : size(len) {
        Alloc(len);
    }

    ScopedBuffer(const T* src, size_t len) : size(len) {
        Alloc(len);
        memcpy(ptr, src, len * sizeof(T));
    }

    ScopedBuffer(ScopedBuffer&& other) noexcept {
        size = other.size;
        if (size <= OPT_SIZE) {
            ptr = buf;
            memcpy(buf, other.buf, size);
        } else {
            ptr = other.ptr;
            other.ptr = nullptr;
        }

        other.size = 0;
    }

    ~ScopedBuffer() {
        if (size > OPT_SIZE) {
            assert(ptr);
            free(ptr);
            ptr = nullptr;
        }
    }

    size_t bytes() const { return size * sizeof(T); }

    operator void* () {
        return (void*)ptr;
    }

    operator T* () {
        return ptr;
    }

    T operator[](size_t n) const {
        assert(n < size);
        return ptr[n];
    }

    template<typename U>
    operator U* () {
        return (U*)ptr;
    }

    T* ptr;
    size_t size;
    T buf[OPT_SIZE];

private:
    void Alloc(size_t len) {
        if (len <= OPT_SIZE) {
            ptr = buf;
        } else {
            ptr = (T*)malloc(len * sizeof(T));
        }
    }
};

using LocalBuffer = ScopedBuffer<uint8_t>;

}
