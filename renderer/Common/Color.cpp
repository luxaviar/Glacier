#include "color.h"

namespace glacier {

const ColorRGBA32 ColorRGBA32::kWhite = { 255, 255, 255, 255 };
const ColorRGBA32 ColorRGBA32::kBlack = { 0, 0, 0, 255 };
const ColorRGBA32 ColorRGBA32::kRed = { 255, 0, 0, 255 };
const ColorRGBA32 ColorRGBA32::kGreen = { 0, 255, 0, 255 };
const ColorRGBA32 ColorRGBA32::kBlue = { 0, 0, 255, 255 };
const ColorRGBA32 ColorRGBA32::kGrey = { 127, 127, 127, 255 };
const ColorRGBA32 ColorRGBA32::kMagenta = { 255, 0, 255, 255 };
const ColorRGBA32 ColorRGBA32::kYellow = { 255, (uint8_t)(0.92f * 255), (uint8_t)(0.015f * 255), 255 };
const ColorRGBA32 ColorRGBA32::kCyan = { 0, 255, 255, 255 };

const Color Color::kWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
const Color Color::kBlack = { 0.0f, 0.0f, 0.0f, 1.0f };
const Color Color::kRed = { 1.0f, 0.0f, 0.0f, 1.0f };
const Color Color::kGreen = { 0.0f, 1.0f, 0.0f, 1.0f };
const Color Color::kBlue = { 0.0f, 0.0f, 1.0f, 1.0f };
const Color Color::kGrey = { 0.5f, 0.5f, 0.5f, 1.0f };
const Color Color::kMagenta = { 1.0f, 0.0f, 1.0f, 1.0f };
const Color Color::kYellow = { 1.0f, 0.92f, 0.016f, 1.0f };
const Color Color::kCyan = { 0.0f, 1.0f, 1.0f, 1.0f };
const Color Color::kOrange = { 1.0f, 0.647058845f, 0.0f, 1.0f };
const Color Color::kIndigo = { 0.294117659f, 0.0f, 0.509803951f, 1.0f };
const Color Color::kViolet = { 0.933333397f, 0.509803951f, 0.933333397f, 1.0f };

ColorRGBA32::ColorRGBA32(const Color& color) :
    ColorRGBA32(
    (uint8_t)math::Clamp(color.r * 255.0f, 0, 255),
    (uint8_t)math::Clamp(color.g * 255.0f, 0, 255),
    (uint8_t)math::Clamp(color.b * 255.0f, 0, 255),
    (uint8_t)math::Clamp(color.a * 255.0f, 0, 255)
    )
{

}

ColorRGBA32 ColorRGBA32::ToGamma() const {
    return LinearToGammaSpace(*this);
}

ColorRGBA32 ColorRGBA32::ToLinear() const {
    return GammaToLinearSpace(*this);
}

Color::Color(const ColorRGBA32& color) :
    Color(
        math::Clamp(color.r / 255.0f, 0.0f, 1.0f),
        math::Clamp(color.g / 255.0f, 0.0f, 1.0f),
        math::Clamp(color.b / 255.0f, 0.0f, 1.0f),
        math::Clamp(color.a / 255.0f, 0.0f, 1.0f)
    )
{
}

Color Color::ToGamma() const {
    return LinearToGammaSpace(*this);
}

Color Color::ToLinear() const {
    return GammaToLinearSpace(*this);
}

}
