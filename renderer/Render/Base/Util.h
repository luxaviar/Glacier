#pragma once

#include "render/base/enums.h"
#include <dxgiformat.h>

namespace glacier {
namespace render {

DXGI_FORMAT GetUnderlyingFormat(TextureFormat format);

DXGI_FORMAT GetSRGBFormat(DXGI_FORMAT format);

DXGI_FORMAT GetSRVFormat(DXGI_FORMAT format);
DXGI_FORMAT GetDSVFormat(DXGI_FORMAT format);
DXGI_FORMAT GetUAVCompatableFormat(DXGI_FORMAT format);

TextureFormat GetTextureFormat(DXGI_FORMAT fmt);

bool IsBGRFormat(DXGI_FORMAT format);
bool IsSRGBFormat(DXGI_FORMAT format);

size_t BitsPerPixel(DXGI_FORMAT fmt);


}
}