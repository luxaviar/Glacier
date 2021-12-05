#pragma once

namespace glacier {
namespace render {

struct ChannelMask {
    ChannelMask(uint64_t v) : mask(v) {}

    friend ChannelMask operator&(ChannelMask lhs, ChannelMask rhs) {
        return { lhs.mask & rhs.mask };
    }

    operator bool() {
        return mask != 0;
    }

    uint64_t mask;
};

struct Channel {
    struct Value {
        operator ChannelMask() const {
            return { 1llu << (int)v };
        }

        operator int() const {
            return v;
        }

        friend ChannelMask operator|(ChannelMask lhs, Value rhs) {
            return { lhs.mask | (1llu << (int)rhs.v) };
        }

        friend ChannelMask operator|(Value lhs, ChannelMask rhs) {
            return { rhs.mask | (1llu << (int)lhs.v) };
        }

        friend ChannelMask operator|(Value lhs, Value rhs) {
            return { (1llu << (int)rhs.v) | (1llu << (int)lhs.v) };
        }

        uint8_t v;
    //private:
    //    friend class Channel;
    //    constexpr Value(uint8_t v) : v(v) {}
    };

    constexpr Channel(Value v) : value(v) {}

    Value value;

    constexpr static Value kMain{ 0 };
    constexpr static Value kShadow{ 1 };
    constexpr static Value kMax{ 2 };
};

}
}
