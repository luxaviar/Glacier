#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <type_traits>

namespace glacier {

template<typename T, typename = std::enable_if_t<std::is_integral<T>::value && std::is_unsigned<T>::value>>
T bit_mask_low(size_t bits) {
    return ((T)1 << bits) - 1;
}

template<typename T, typename = std::enable_if_t<std::is_integral<T>::value && std::is_unsigned<T>::value>>
T bit_mask_high(size_t bits) {
    return (((T)1 << bits) - 1) << (sizeof(T) * 4 - bits);
}


//Calculates the population count of an unsigned 32 bit integer at compile time.
//Population count is the number of bits in the integer that set to 1.
//See "Hacker's Delight" and http://www.hackersdelight.org/hdcodetxt/popArrayHS.c.txt
template <uint32_t x>
struct static_popcount {
    enum {
        a = x - ( ( x >> 1 )       & 0x55555555 ),
        b =   ( ( ( a >> 2 )       & 0x33333333 ) + ( a & 0x33333333 ) ),
        c =   ( ( ( b >> 4 ) + b ) & 0x0f0f0f0f ),
        d =   c + ( c >> 8 ),
        e =   d + ( d >> 16 ),
        result = e & 0x0000003f
    };
};

//Calculates the log 2 of an unsigned 32 bit integer at compile time.
template <uint32_t x>
struct static_log2 {
    enum {
        a = x | (x >> 1),
        b = a | (a >> 2),
        c = b | (b >> 4),
        d = c | (c >> 8),
        e = d | (d >> 16),
        f = e >> 1,
        result = static_popcount<f>::result
    };
};

//Calculates the number of bits required to serialize an integer value in [min,max] at compile time.
template <int64_t min, int64_t max>
struct static_bits_required {
    static const uint32_t result = (min == max) ? 0 : (static_log2<uint32_t(max-min)>::result + 1);
};


//Calculates the population count of an unsigned 32 bit integer.
//The population count is the number of bits in the integer set to 1.
inline constexpr uint32_t popcount( uint32_t x ) {
#ifdef __GNUC__
    return __builtin_popcount( x );
#else // #ifdef __GNUC__
    const uint32_t a = x - ( ( x >> 1 )       & 0x55555555 );
    const uint32_t b =   ( ( ( a >> 2 )       & 0x33333333 ) + ( a & 0x33333333 ) );
    const uint32_t c =   ( ( ( b >> 4 ) + b ) & 0x0f0f0f0f );
    const uint32_t d =   c + ( c >> 8 );
    const uint32_t e =   d + ( d >> 16 );
    const uint32_t result = e & 0x0000003f;
    return result;
#endif // #ifdef __GNUC__
}

//Calculates the log base 2 of an unsigned 32 bit integer.
//7 -> 2, 8 -> 3, 9 -> 3
inline constexpr uint32_t log2( uint32_t x ) {
    const uint32_t a = x | (x >> 1);
    const uint32_t b = a | (a >> 2);
    const uint32_t c = b | (b >> 4);
    const uint32_t d = c | (c >> 8);
    const uint32_t e = d | (d >> 16);
    const uint32_t f = e >> 1;
    return popcount(f);
}

//7 -> 8, 8 -> 8, 9 -> 16
inline constexpr uint32_t next_power_of2(uint32_t v) {
    if (v == 0) return 1;

    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

inline constexpr uint32_t next_log_of2(uint32_t v) {
    return log2(next_power_of2(v));
}

//Calculates the number of bits required to serialize an integer in range [min,max].
inline int bits_required(uint32_t min, uint32_t max) {
#ifdef __GNUC__
    return (min == max) ? 0 : 32 - __builtin_clz(max - min);
#else // #ifdef __GNUC__
    return (min == max) ? 0 : log2(max - min) + 1;
#endif // #ifdef __GNUC__
}

//Reverse the order of bytes in a 64 bit integer.
inline uint64_t bswap(uint64_t value) {
#ifdef __GNUC__
    return __builtin_bswap64( value );
#else // #ifdef __GNUC__
    value = (value & 0x00000000FFFFFFFF) << 32 | (value & 0xFFFFFFFF00000000) >> 32;
    value = (value & 0x0000FFFF0000FFFF) << 16 | (value & 0xFFFF0000FFFF0000) >> 16;
    value = (value & 0x00FF00FF00FF00FF) << 8  | (value & 0xFF00FF00FF00FF00) >> 8;
    return value;
#endif // #ifdef __GNUC__
}

//Reverse the order of bytes in a 32 bit integer.
inline uint32_t bswap(uint32_t value) {
#ifdef __GNUC__
    return __builtin_bswap32(value);
#else // #ifdef __GNUC__
    return (value & 0x000000ff) << 24 | (value & 0x0000ff00) << 8 | (value & 0x00ff0000) >> 8 | (value & 0xff000000) >> 24;
#endif // #ifdef __GNUC__
}

//Reverse the order of bytes in a 16 bit integer.
inline uint16_t bswap(uint16_t value) {
    return (value & 0x00ff) << 8 | (value & 0xff00) >> 8;
}

//Convert a signed integer to an unsigned integer with zig-zag encoding.
//0,-1,+1,-2,+2... becomes 0,1,2,3,4 ...
inline int zigzag_to_unsigned( int n ) {
    return ( n << 1 ) ^ ( n >> 31 );
}

//Convert an unsigned integer to as signed integer with zig-zag encoding.
//0,1,2,3,4... becomes 0,-1,+1,-2,+2...
inline int unsigned_to_zigzag( uint32_t n ) {
    return ( n >> 1 ) ^ ( -int32_t( n & 1 ) );
}

}
