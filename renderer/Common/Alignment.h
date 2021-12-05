#pragma once

#include <cassert>
#include "Math/Util.h"

namespace glacier
{

template<size_t alignment, typename T>
inline constexpr T AlignUp(T val) noexcept
{
    assert(IsPowOf2(alignment));
    return (val + alignment - 1) & ~(alignment - 1);
}

template<size_t alignment, typename T>
inline constexpr T AlignDown(T val) noexcept
{
    assert(IsPowOf2(alignment));
    return val & ~(alignment - 1);
}

inline size_t AlignBoundary(size_t offset, size_t alignment) noexcept
{
    return ((offset + alignment - 1) / alignment) * alignment;
}

// Aligns a value to the nearest higher multiple of 'align_size'.
template<size_t alignment = 16>
size_t AlignBoundary(size_t offset) noexcept
{
    return ((offset + alignment - 1) / alignment) * alignment;
}

template<size_t alignment = 16>
bool IsCrossesBoundary(size_t offset, size_t size) noexcept
{
    const auto expect_offset = offset + size;
    const auto align_start = offset / alignment;
    const auto align_end = expect_offset / alignment;
    return (align_start != align_end && expect_offset % alignment != 0) || size > alignment;
}

//allocate size bytes at offset
template<size_t alignment = 16>
size_t AlignOffset(size_t offset, size_t size) noexcept
{
    return IsCrossesBoundary(offset, size) ? AlignBoundary(offset) : offset;
}

}
