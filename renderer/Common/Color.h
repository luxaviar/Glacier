#pragma once

#include "Math/Vec4.h"

namespace glacier {

struct Color;

struct ColorRGBA32 : public Vec4<uint8_t> {
    constexpr ColorRGBA32() : Vec4<uint8_t>(255) {}
    constexpr ColorRGBA32(uint8_t r, uint8_t g, uint8_t b, uint8_t a=255) : Vec4<uint8_t>(r, g, b, a) {}
    constexpr ColorRGBA32(const ColorRGBA32& color) = default;
    ColorRGBA32(const Color & color);

    ColorRGBA32 ToGamma() const;
    ColorRGBA32 ToLinear() const;

    const static ColorRGBA32 kWhite;
    const static ColorRGBA32 kBlack;
    const static ColorRGBA32 kRed;
    const static ColorRGBA32 kGreen;
    const static ColorRGBA32 kBlue;
    const static ColorRGBA32 kGrey;
    const static ColorRGBA32 kMagenta;
    const static ColorRGBA32 kYellow;
    const static ColorRGBA32 kCyan;
};

struct Color : public Vec4<float> {
    constexpr Color() : Vec4<float>(1.0f) {}
    constexpr Color(float r, float g, float b, float a=1.0f) : Vec4<float>(r, g, b, a) {}
    constexpr Color(const Color& color) = default;
    Color(const ColorRGBA32 & color);

    Color ToGamma() const;
    Color ToLinear() const;

    const static Color kWhite;
    const static Color kBlack;
    const static Color kRed;
    const static Color kGreen;
    const static Color kBlue;
    const static Color kGrey;
    const static Color kMagenta;
    const static Color kYellow;
    const static Color kCyan;
    const static Color kOrange;
    const static Color kIndigo;
    const static Color kViolet;
};

inline float ColorToGreyValue(const Color& color) {
    return color.r * 0.3f + color.g * 0.59f + color.b * 0.11f;
}

// http://www.opengl.org/registry/specs/EXT/framebuffer_sRGB.txt
// http://www.opengl.org/registry/specs/EXT/texture_sRGB_decode.txt
// {  cs / 12.92,                 cs <= 0.04045 }
// {  ((cs + 0.055)/1.055)^2.4,   cs >  0.04045 }

inline float GammaToLinearSpace (float value)
{
    if (value <= 0.04045F)
        return value / 12.92F;
    else if (value < 1.0F)
        return pow((value + 0.055F)/1.055F, 2.4F);
    else
        return pow(value, 2.4F);
}

// http://www.opengl.org/registry/specs/EXT/framebuffer_sRGB.txt
// http://www.opengl.org/registry/specs/EXT/texture_sRGB_decode.txt
// {  0.0,                          0         <= cl
// {  12.92 * c,                    0         <  cl < 0.0031308
// {  1.055 * cl^0.41666 - 0.055,   0.0031308 <= cl < 1
// {  1.0,                                       cl >= 1  <- This has been adjusted since we want to maintain HDR colors

inline float LinearToGammaSpace (float value)
{
    if (value <= 0.0F)
        return 0.0F;
    else if (value <= 0.0031308F)
        return 12.92F * value;
    else if (value <= 1.0F)
        return 1.055F * powf(value, 0.41666F) - 0.055F;
    else
        return powf(value, 0.41666F);
}

inline float GammaToLinearSpaceXenon(float val)
{
    float ret;
    if (val < 0)
        ret = 0;
    else if (val < 0.25f)
        ret = 0.25f * val;
    else if (val < 0.375f)
        ret = (1.0f/16.0f) + 0.5f*(val-0.25f);
    else if (val < 0.75f)
        ret = 0.125f + 1.0f*(val-0.375f);
    else if (val < 1.0f)
        ret = 0.5f + 2.0f*(val-0.75f);
    else
        ret = 1.0f;
    return ret;
}

inline float LinearToGammaSpaceXenon(float val)
{
    float ret;
    if (val < 0)
        ret = 0;
    else if (val < (1.0f/16.0f))
        ret = 4.0f * val;
    else if (val < (1.0f/8.0f))
        ret = (1.0f/4.0f) + 2.0f*(val-(1.0f/16.0f));
    else if (val < 0.5f)
        ret = 0.375f + 1.0f*(val-0.125f);
    else if (val < 1.0f)
        ret = 0.75f + 0.5f*(val-0.50f);
    else
        ret = 1.0f;
    
    return ret;
}

inline Color GammaToLinearSpace (const Color& value) {
    return Color(GammaToLinearSpace(value.r), GammaToLinearSpace(value.g), GammaToLinearSpace(value.b), value.a);
}

inline Color LinearToGammaSpace (const Color& value) {
    return Color(LinearToGammaSpace(value.r), LinearToGammaSpace(value.g), LinearToGammaSpace(value.b), value.a);
}

inline Color GammaToLinearSpaceXenon (const Color& value)
{
    return Color(GammaToLinearSpaceXenon(value.r), GammaToLinearSpaceXenon(value.g), GammaToLinearSpaceXenon(value.b), value.a);
}

inline Color LinearToGammaSpaceXenon (const Color& value)
{
    return Color(LinearToGammaSpaceXenon(value.r), LinearToGammaSpaceXenon(value.g), LinearToGammaSpaceXenon(value.b), value.a);
}

}
