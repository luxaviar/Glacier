#pragma once

#include <algorithm>
#include <functional>
#include "resource.h"
#include "common/color.h"
#include "Common/Util.h"

namespace glacier {
namespace render {

class Image;
class Texture;

struct TextureDescription {
    TextureDescription& EnableSRGB(bool v = true) { srgb = v; return *this; }
    TextureDescription& EnableMips(bool v = true) { gen_mips = v; return *this; }
    TextureDescription& SetFormat(TextureFormat fmt) { format = fmt; return *this; }
    TextureDescription& SetFile(const TCHAR* file_) { file = file_; return *this; }
    TextureDescription& SetDimension(uint32_t w, uint32_t h) { width = w; height = h; return *this; }
    TextureDescription& SetColor(const Color& color_) { color = color_; use_color = true; return *this; }
    TextureDescription& SetSampleDesc(uint32_t count, uint32_t qulity) { sample_count = count; sample_quality = qulity; return *this; }
    TextureDescription& SetType(TextureType type_) {
        type = type_;
        if (type == TextureType::kTextureCube) {
            depth_or_array_size = 6;
        }

        return *this;
    }
    TextureDescription& SetCreateFlag(CreateFlags flags) { create_flags |= (uint32_t)flags; return *this; }

    bool srgb = false;
    bool gen_mips = false;
    TextureFormat format = TextureFormat::kUnkown;
    uint32_t create_flags = 0;
    TextureType type = TextureType::kTexture2D;

    //int backbuffer_index = -1;
    bool is_backbuffer = false;
    bool use_color;
    Color color;
    EngineString file;

    uint32_t width;
    uint32_t height;
    uint32_t sample_count = 1;
    uint32_t sample_quality = 0;
    uint32_t depth_or_array_size = 1;
};

class Texture : public Resource {
public:
    using ReadBackResolver = std::function<size_t(const uint8_t* texel, ColorRGBA32* pixel)>;
    using ReadbackDelegate = std::function<void(const uint8_t* data, size_t raw_pitch)>;

    static TextureDescription Description() { return TextureDescription{}; }

    static uint32_t CalcNumberOfMipLevels(uint32_t width, uint32_t height) noexcept {
        return (uint32_t)std::floor(std::log2(std::max(width, height))) + 1;
    }

    static uint32_t CalcSubresource(uint32_t mip_slice, uint32_t array_slice, uint32_t mip_levels) {
        return mip_slice + array_slice * mip_levels;
    }

    Texture(const TextureDescription& desc) : 
        detail_(desc)
    {}

    uint32_t width() const { return detail_.width; }
    uint32_t height() const { return detail_.height; }
    
    virtual uint32_t GetMipLevels() const = 0;
    TextureFormat GetFormat() const { return detail_.format; }

    uint32_t GetSampleCount() const { return detail_.sample_count; }
    uint32_t GetSampleQuality() const { return detail_.sample_quality; }

    bool IsCubeMap() const { return detail_.type == TextureType::kTextureCube; }

    virtual void GenerateMipMaps() = 0;

    virtual void ReleaseUnderlyingResource() = 0;

    virtual bool Resize(uint32_t width, uint32_t height) = 0;

    virtual void ReadBackImage(int left, int top,
        int width, int height, int destX, int destY, ReadbackDelegate&& callback) = 0;

protected:
    TextureDescription detail_;
};

}
}
