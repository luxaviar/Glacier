#include "Bytestream.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "Log.h"

namespace glacier {

ByteStream::ByteStream() : ByteStream(kInitCapacity - kPrependSize) {
}

ByteStream::ByteStream(size_t append_size) : ByteStream(kPrependSize, append_size) {
}

ByteStream::ByteStream(size_t prepend_size, size_t append_size) : 
    init_prepend_size_(prepend_size), 
    capacity_(prepend_size + append_size),
    reader_idx_(prepend_size), 
    writer_idx_(prepend_size) 
{
    src_ = (char*)malloc(capacity_);
}

ByteStream::ByteStream(ByteStream&& other) noexcept {
    src_ = other.src_;
    capacity_ = other.capacity_;
    reader_idx_ = other.reader_idx_;
    writer_idx_ = other.writer_idx_;
    init_prepend_size_ = other.init_prepend_size_;

    other.src_ = nullptr;
    other.capacity_ = 0;
    other.reader_idx_ = 0;
    other.writer_idx_ = 0;
}

ByteStream::~ByteStream() {
    if (!src_) return;

    free(src_);
    src_ = nullptr;
    capacity_ = 0;
    reader_idx_ = 0;
    writer_idx_ = 0;
}

void ByteStream::Reset() {
    reader_idx_ = init_prepend_size_;
    writer_idx_ = init_prepend_size_;
}

void ByteStream::Drain(size_t size) {
    if (size > ReadableBytes()) {
        Reset();
        return;
    }

    reader_idx_ += size;
}

size_t ByteStream::Fill(size_t size) {
    if (size > WritableBytes()) {
        size = WritableBytes();
    }

    writer_idx_ += size;
    return size;
}

void ByteStream::DrainBack(size_t size) {
    if (size > ReadableBytes()) {
        Reset();
        return;
    }

    writer_idx_ -= size;
}

int ByteStream::Prepend(const void* data, size_t size) {
    assert(PrependableBytes() >= size);
    if (PrependableBytes() < size) {
        LOG_ERR("ByteStream::Prepend no enough space to prepend, expect {}, but current {}", size, PrependableBytes());
        return -1;
    }

    reader_idx_ -= size;
    memcpy(src_ + reader_idx_, data, size);
    return 0;
}

void ByteStream::Write(const void* data, size_t size) {
    if (size == 0) return;

    MakeSpace(size);

    memcpy(src_ + writer_idx_, data, size);
    writer_idx_ += size;
}

void ByteStream::Write(ByteStream& stream) {
    Write(stream.rbegin(), stream.ReadableBytes());
}

int ByteStream::Read(void* data, size_t size) {
    if (size == 0) return 0;

    if (ReadableBytes() < size) {
        read_fail_ = true;
        LOG_ERR("ByteStream::read , no enough data to read, expected {}, but current length {}", size, ReadableBytes());
        return 0;
    }

    memcpy(data, src_ + reader_idx_, size);
    reader_idx_ += size;

    if (reader_idx_ == writer_idx_) {
        Shrink();
    }

    return size;
}

int ByteStream::Peek(void* data, size_t size, size_t offset) {
    if (size == 0) return 0;

    if (ReadableBytes() < size + offset) {
        LOG_ERR("ByteStream::read , no enough data to peek (at offset {}, expected {}, but current length {}", offset, size + offset, ReadableBytes());
        return -1;
    }

    memcpy(data, src_ + reader_idx_ + offset, size);
    return size;
}

void ByteStream::Shrink() {
    if (reader_idx_ == writer_idx_) {
        Reset();
    }

    if (reader_idx_ == init_prepend_size_) {
        return;
    }

    size_t size = ReadableBytes();
    memmove(src_ + init_prepend_size_, src_ + reader_idx_, size);
    reader_idx_ = init_prepend_size_;
    writer_idx_ = init_prepend_size_ + size;
    return;
}

void ByteStream::Resize(size_t new_capacity) {
    ASSERT(new_capacity >= capacity_);
    char* new_src = (char*)malloc(new_capacity);
    size_t size = ReadableBytes();
    if (size > 0)
        memcpy(new_src + init_prepend_size_, src_ + reader_idx_, size);

    free(src_);
    src_ = new_src;
    capacity_ = new_capacity;
    reader_idx_ = init_prepend_size_;
    writer_idx_ = init_prepend_size_ + size;
}

void ByteStream::Expand(size_t size) {
    size_t new_size = ReadableBytes() + size + init_prepend_size_; //the minimum size for readable data & extra writing requirement;
    if (new_size <= capacity_) {
        Shrink();
        return;
    }

    size_t next_capacity = capacity_;
    next_capacity += (capacity_ + 1) / 2;

    if (next_capacity < new_size) {
        new_size += (new_size + 1) / 2;
        next_capacity = new_size;
    }

    Resize(next_capacity);
}

void ByteStream::MakeSpace(size_t size) {
    if (WritableBytes() >= size) return;
    Expand(size);
    assert(WritableBytes() >= size);
}

}
