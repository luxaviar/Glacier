#pragma once

#include <type_traits>
#include <stddef.h>
#include <stdint.h>
#include "Uncopyable.h"

namespace glacier {

class ByteStream : private Uncopyable {
public:
    static constexpr size_t kPrependSize = 32;
    static constexpr size_t kInitCapacity = 1024;

    ByteStream();
    ByteStream(size_t append_size);
    ByteStream(size_t prepend_size, size_t append_size);
    ByteStream(ByteStream&& other) noexcept;
    ~ByteStream();

    void* data() const { return src_; }
    void* rbegin() const { return src_ + reader_idx_; }
    void* wbegin() const { return src_ + writer_idx_; }
    size_t capacity() const { return capacity_; }

    size_t WritableBytes() const { return capacity_ - writer_idx_; }
    size_t ReadableBytes() const { return writer_idx_ - reader_idx_; }
    size_t PrependableBytes() const { return reader_idx_; }

    void Drain(size_t size);
    void DrainBack(size_t size);

    size_t Fill(size_t size);

    void Reset();
    void MakeSpace(size_t size);

    bool IsReadFailed() const { return read_fail_; }
    void ResetReadFailed() { read_fail_ = false; }

    int Prepend(const void* data, size_t size);

    template<typename T,
        typename std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
    void Prepend(const T& t) {
        Prepend(&t, sizeof(T));
    }

    void Write(const void* data, size_t size);
    void Write(ByteStream& stream);

    template<typename T>
    void Write(const T& t) {
        Write(&t, sizeof(T));
    }

    template<typename T,
        typename std::enable_if_t<std::is_integral<T>::value && std::is_unsigned<T>::value, int> = 0>
    void WriteVint(T v) {
        uint8_t data[10];
        int i = 0;
        while (v >= 0x80) {
            data[i++] = (v | 0x80);
            v >>= 7;
        }
        data[i++] = v;
        Write(data, i);
    }

    template<typename T,
        typename std::enable_if_t<std::is_integral<T>::value && std::is_signed<T>::value, int> = 0>
    void WriteSint(T v) {
        auto value = ZigZagEncode(v);
        WriteVint(value);
    }

    int Read(void* data, size_t size);

    template<typename T>
    int Read(T& t) {
        return Read(&t, sizeof(T));
    }

    template<typename T,
        typename std::enable_if_t<std::is_same<T, uint32_t>::value || std::is_same<T, uint64_t>::value, int> = 0>
    int ReadVint(T& t) {
        uint8_t ch;
        int cnt = Read(ch);
        uint64_t value = ch & 0x7f;
        size_t shift = 7;
        while ((ch & 0x80) && ReadableBytes() > 0) {
            cnt += Read(ch);
            value |= ((uint64_t)(ch & 0x7f)) << shift;
            shift += 7;
        }

        t = (T)value;
        return cnt;
    }

    int ReadSint(int32_t& v) {
        uint32_t value;
        int rc = ReadVint(value);
        v = ZigZagDecode(value);
        return rc;
    }

    int ReadSint(int64_t& v) {
        uint64_t value;
        int rc = ReadVint(value);
        v = ZigZagDecode(value);
        return rc;
    }

    int Peek(void* data, size_t size, size_t offset);

    template<typename T,
        typename std::enable_if_t<std::is_trivially_copyable<T>::value, int> = 0>
    int Peek(T& t, size_t offset = 0) {
        return peek(&t, sizeof(T), offset);
    }

    template<typename T>
    ByteStream& operator <<(const T& v) { Write(v); return *this; }

    template<typename T>
    ByteStream& operator >>(T& v) { Read(v); return *this; }

private:
    void Shrink();
    void Resize(size_t new_capacity);
    void Expand(size_t size);

    uint32_t ZigZagEncode(int32_t n) {
        return (n << 1) ^ (n >> 31);
    }

    uint64_t ZigZagEncode(int64_t n) {
        return (n << 1) ^ (n >> 63);
    }

    int32_t ZigZagDecode(uint32_t n) {
        return (n >> 1) ^ -static_cast<int32_t>(n & 1);
    }

    int64_t ZigZagDecode(uint64_t n) {
        return (n >> 1) ^ -static_cast<int64_t>(n & 1);
    }

    bool read_fail_ = false;
    char* src_;
    size_t init_prepend_size_;
    size_t capacity_;

    size_t reader_idx_;
    size_t writer_idx_;
};

}
