#pragma once


#include "Util.h"

namespace glacier {
namespace render {

DXGI_FORMAT GetSRGBFormat(DXGI_FORMAT format) {
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case DXGI_FORMAT_BC1_UNORM:
        return DXGI_FORMAT_BC1_UNORM_SRGB;
    case DXGI_FORMAT_BC2_UNORM:
        return DXGI_FORMAT_BC2_UNORM_SRGB;
    case DXGI_FORMAT_BC3_UNORM:
        return DXGI_FORMAT_BC3_UNORM_SRGB;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case DXGI_FORMAT_B8G8R8X8_UNORM:
        return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
    case DXGI_FORMAT_BC7_UNORM:
        return DXGI_FORMAT_BC7_UNORM_SRGB;
    default:
        return format;
    }
}

DXGI_FORMAT GetUnderlyingFormat(TextureFormat format) {
    switch (format)
    {
    case TextureFormat::kR8G8B8A8_UNORM:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::kR8G8B8A8_UNORM_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case TextureFormat::kR24G8_TYPELESS:
        return DXGI_FORMAT_R24G8_TYPELESS;
    case TextureFormat::kR24X8_TYPELESS:
        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case TextureFormat::kD24S8_UINT:
        return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case TextureFormat::kR32_TYPELESS:
        return DXGI_FORMAT_R32_TYPELESS;
    case TextureFormat::kR32_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;
    case TextureFormat::kD32_FLOAT:
        return DXGI_FORMAT_D32_FLOAT;
    case TextureFormat::kR32G32B32A32_FLOAT:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case TextureFormat::kR32G32B32A32_TYPELESS:
        return DXGI_FORMAT_R32G32B32A32_TYPELESS;
    case TextureFormat::kR16G16B16A16_FLOAT:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case TextureFormat::kR16G16_FLOAT:
        return DXGI_FORMAT_R16G16_FLOAT;
    case TextureFormat::kR16G16B16A16_TYPELESS:
        return DXGI_FORMAT_R16G16B16A16_TYPELESS;
    case TextureFormat::kR16_UNORM:
        return DXGI_FORMAT_R16_UNORM;
    case TextureFormat::kR16G16_UNORM:
        return DXGI_FORMAT_R16G16_UNORM;
    case TextureFormat::kR11G11B10_FLOAT:
        return DXGI_FORMAT_R11G11B10_FLOAT;
    default:
        throw std::exception{ "Bad TextureFormat." };
    }
}

DXGI_FORMAT GetSRVFormat(DXGI_FORMAT format) {
    DXGI_FORMAT result = format;
    switch (format)
    {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        result = DXGI_FORMAT_R32G32B32A32_FLOAT;
        break;
    case DXGI_FORMAT_R32G32B32_TYPELESS:
        result = DXGI_FORMAT_R32G32B32_FLOAT;
        break;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        result = DXGI_FORMAT_R16G16B16A16_UNORM;
        break;
    case DXGI_FORMAT_R32G32_TYPELESS:
        result = DXGI_FORMAT_R32G32_FLOAT;
        break;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        result = DXGI_FORMAT_R10G10B10A2_UNORM;
        break;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        result = DXGI_FORMAT_R8G8B8A8_UNORM;
        break;
    case DXGI_FORMAT_R16G16_TYPELESS:
        result = DXGI_FORMAT_R16G16_UNORM;
        break;
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_D16_UNORM:
        result = DXGI_FORMAT_R16_UNORM;
        break;
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        result = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        break;
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
        result = DXGI_FORMAT_R32_FLOAT;
        break;
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        result = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
        break;
    case DXGI_FORMAT_R8G8_TYPELESS:
        result = DXGI_FORMAT_R8G8_UNORM;
        break;
    case DXGI_FORMAT_R8_TYPELESS:
        result = DXGI_FORMAT_R8_UNORM;
        break;
    }

    return result;
}

DXGI_FORMAT GetDSVFormat(DXGI_FORMAT format)
{
    DXGI_FORMAT result = format;

    switch (format)
    {
    case DXGI_FORMAT_R16_TYPELESS:
        result = DXGI_FORMAT_D16_UNORM;
        break;
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        result = DXGI_FORMAT_D24_UNORM_S8_UINT;
        break;
    case DXGI_FORMAT_R32_TYPELESS:
        result = DXGI_FORMAT_D32_FLOAT;
        break;
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        result = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        break;
    }

    return result;
}

DXGI_FORMAT GetUAVCompatableFormat( DXGI_FORMAT format ) {
    DXGI_FORMAT uavFormat = format;

    switch ( format )
    {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        uavFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        break;
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        uavFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
        break;
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
        uavFormat = DXGI_FORMAT_R32_FLOAT;
        break;
    }

    return uavFormat;
}

bool IsBGRFormat(DXGI_FORMAT format) {
    switch (format)
    {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        return true;
    default:
        return false;
    }
}

bool IsSRGBFormat(DXGI_FORMAT format) {
    switch (format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return true;
    default:
        return false;
    }
}

size_t BitsPerPixel(DXGI_FORMAT fmt)
{
    switch (static_cast<int>(fmt))
    {
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
        return 128;

    case DXGI_FORMAT_R32G32B32_TYPELESS:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
        return 96;

    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R32G32_TYPELESS:
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
    case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
    case DXGI_FORMAT_Y416:
    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
        return 64;

    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
    case DXGI_FORMAT_R11G11B10_FLOAT:
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R16G16_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_R32_FLOAT:
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
    case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
    case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8X8_UNORM:
    case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
    case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
    case DXGI_FORMAT_AYUV:
    case DXGI_FORMAT_Y410:
    case DXGI_FORMAT_YUY2:
    //case XBOX_DXGI_FORMAT_R10G10B10_7E3_A2_FLOAT:
    //case XBOX_DXGI_FORMAT_R10G10B10_6E4_A2_FLOAT:
    //case XBOX_DXGI_FORMAT_R10G10B10_SNORM_A2_UNORM:
        return 32;

    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
    //case XBOX_DXGI_FORMAT_D16_UNORM_S8_UINT:
    //case XBOX_DXGI_FORMAT_R16_UNORM_X8_TYPELESS:
    //case XBOX_DXGI_FORMAT_X16_TYPELESS_G8_UINT:
    //case WIN10_DXGI_FORMAT_V408:
        return 24;

    case DXGI_FORMAT_R8G8_TYPELESS:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_SINT:
    case DXGI_FORMAT_R16_TYPELESS:
    case DXGI_FORMAT_R16_FLOAT:
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_B5G6R5_UNORM:
    case DXGI_FORMAT_B5G5R5A1_UNORM:
    case DXGI_FORMAT_A8P8:
    case DXGI_FORMAT_B4G4R4A4_UNORM:
    //case WIN10_DXGI_FORMAT_P208:
    //case WIN10_DXGI_FORMAT_V208:
        return 16;

    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_420_OPAQUE:
    case DXGI_FORMAT_NV11:
        return 12;

    case DXGI_FORMAT_R8_TYPELESS:
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_SINT:
    case DXGI_FORMAT_A8_UNORM:
    case DXGI_FORMAT_AI44:
    case DXGI_FORMAT_IA44:
    case DXGI_FORMAT_P8:
    //case XBOX_DXGI_FORMAT_R4G4_UNORM:
        return 8;

    case DXGI_FORMAT_R1_UNORM:
        return 1;

    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        return 4;

    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return 8;

    default:
        return 0;
    }
}

TextureFormat GetTextureFormat(DXGI_FORMAT format) {
    switch (format)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
        return TextureFormat::kR8G8B8A8_UNORM;
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        return TextureFormat::kR8G8B8A8_UNORM_SRGB;
    case DXGI_FORMAT_R24G8_TYPELESS:
        return TextureFormat::kR24G8_TYPELESS;
    case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        return TextureFormat::kR24X8_TYPELESS;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        return TextureFormat::kD24S8_UINT;
    case DXGI_FORMAT_R32_TYPELESS:
        return TextureFormat::kR32_TYPELESS;
    case DXGI_FORMAT_R32_FLOAT:
        return TextureFormat::kR32_FLOAT;
    case DXGI_FORMAT_D32_FLOAT:
        return TextureFormat::kD32_FLOAT;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return TextureFormat::kR32G32B32A32_FLOAT;
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        return TextureFormat::kR32G32B32A32_TYPELESS;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return TextureFormat::kR16G16B16A16_FLOAT;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        return TextureFormat::kR16G16B16A16_TYPELESS;
    case DXGI_FORMAT_R16G16_FLOAT:
        return TextureFormat::kR16G16_FLOAT;
    case DXGI_FORMAT_R16G16_UNORM:
        return TextureFormat::kR16G16_UNORM;
    case DXGI_FORMAT_R16_UNORM:
        return TextureFormat::kR16_UNORM;
    case DXGI_FORMAT_R11G11B10_FLOAT:
        return TextureFormat::kR11G11B10_FLOAT;
    default:
        throw std::exception{ "Bad TextureFormat." };
    }
}

}
}