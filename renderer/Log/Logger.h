#pragma once

#include <string>
#include <sstream>
#include <string.h>
#include <stdint.h>
#include "Common/Uncopyable.h"
#include "Math/Vec2.h"
#include "Math/Vec3.h"
#include "fmt/format.h"
#include "Common/ByteStream.h"

namespace glacier {

class BaseLogger : private Uncopyable {
public:
    BaseLogger() {}
    virtual ~BaseLogger() {}

    virtual void Append(int8_t level, ByteStream& stream) = 0;
    virtual void Flush() {}
    virtual void Write(ByteStream& stream) = 0;

protected:
    virtual void Close() {}
};

class ConsoleLogger : public BaseLogger {
public:
    void Append(int8_t level, ByteStream& stream) override;
    void Write(ByteStream& stream) override;
};

class FileLogger : public BaseLogger {
public:
    FileLogger(const char* file);
    ~FileLogger() override;

    void Append(int8_t level, ByteStream& stream) override;
    void Write(ByteStream& stream) override;

    void Flush() override;

protected:
    void Open();
    void Close() override;

    std::string file_path_;
    FILE* file_ = nullptr;
    char buffer_[64 * 1024];
    bool dup_std_err_;
};

struct HexMemoryView {
    HexMemoryView(const void* src, size_t len) : src(src), len(len) {}
    const void* src;
    size_t len;
};

struct BinMemoryView {
    BinMemoryView(const void* src, size_t len) : src(src), len(len) {}
    const void* src;
    size_t len;
};

}

template<typename T>
struct fmt::formatter<glacier::Vec2<T>> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const glacier::Vec2<T>& v, FormatContext& ctx) -> decltype(ctx.out()) {
        format_to(ctx.out(), "[{}, {}]", v.x, v.y);
        return ctx.out();
    }
};

template<typename T>
struct fmt::formatter<glacier::Vec3<T>> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const glacier::Vec3<T>& v, FormatContext& ctx) -> decltype(ctx.out()) {
        format_to(ctx.out(), "[{}, {}, {}]", v.x, v.y, v.z);
        return ctx.out();
    }
};

template<>
struct fmt::formatter<glacier::HexMemoryView> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const glacier::HexMemoryView& v, FormatContext& ctx) -> decltype(ctx.out()) {
        constexpr int kLineCharNum = 16;
        unsigned char* src = (unsigned char*)v.src;
        int len = (int)v.len;

        int pos = 0;
        int line = 0;
        char str[kLineCharNum];
        auto&& out = ctx.out();
        for (; len > 0; len -= kLineCharNum) {
            format_to(out, "{:08X}", line);
            size_t line_length = std::min(len, kLineCharNum);
            int i;
            for (i = 0; i < line_length; ++i) {
                auto c = *(src + pos);
                pos++;
                format_to(out, " {:02X}", c);

                if (c >= '!' && c <= '~') {
                    str[i] = c;
                }
                else {
                    str[i] = '.';
                }
            }

            for (; i < kLineCharNum; ++i) {
                format_to(out, "   ");
            }

            format_to(out, " ; {}\n", std::string_view(str, line_length));
            line++;
        }

        return out;
    }
};

template<>
struct fmt::formatter<glacier::BinMemoryView> {
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const glacier::BinMemoryView& v, FormatContext& ctx) -> decltype(ctx.out()) {
        constexpr int kLineCharNum = 16;
        unsigned char* src = (unsigned char*)v.src;
        int len = (int)v.len;

        int pos = 0;
        int line = 0;
        char str[kLineCharNum];
        char bin[9];
        auto&& out = ctx.out();
        for (; len > 0; len -= kLineCharNum) {
            format_to(out, "{:08X}", line);
            size_t line_length = std::min(len, kLineCharNum);
            int i;
            for (i = 0; i < line_length; ++i) {
                auto c = *(src + pos);
                pos++;
                format_to(out, " {:08B}", c);

                if (c >= '!' && c <= '~') {
                    str[i] = c;
                }
                else {
                    str[i] = '.';
                }
            }

            for (; i < kLineCharNum; ++i) {
                format_to(out, "         ");
            }

            format_to(out, " ; {}\n", std::string_view(str, line_length));
            line++;
        }

        return out;
    }
};
