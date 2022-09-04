#pragma once

#include <algorithm>
#include <functional>
#include <array>
#include "resource.h"
#include "common/color.h"
#include "Common/Util.h"

namespace glacier {
namespace render {

class Image;
class Texture;
class CommandBuffer;

struct TextureDescription {
    TextureDescription& EnableSRGB(bool v = true) { srgb = v; return *this; }
    TextureDescription& EnableMips(bool v = true) { gen_mips = v; return *this; }
    TextureDescription& SetFormat(TextureFormat fmt) { format = fmt; return *this; }
    TextureDescription& SetDimension(uint32_t w, uint32_t h) { width = w; height = h; return *this; }
    TextureDescription& SetSampleDesc(uint32_t count, uint32_t qulity) { sample_count = count; sample_quality = qulity; return *this; }
    TextureDescription& SetType(TextureType type_) {
        type = type_;
        if (type == TextureType::kTextureCube) {
            depth_or_array_size = 6;
        }

        return *this;
    }
    TextureDescription& SetCreateFlag(CreateFlags flags) { create_flags |= (uint32_t)flags; return *this; }
    TextureDescription& SetCreateFlag(uint32_t flags) { create_flags = flags; return *this; }

    bool srgb = false;
    bool gen_mips = false;
    TextureFormat format = TextureFormat::kUnkown;
    uint32_t create_flags = 0;
    TextureType type = TextureType::kTexture2D;

    uint32_t width = 8;
    uint32_t height = 8;
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

    Texture() {
        resource_type_ = ResourceType::kTexture;
    }

    virtual uint32_t width() const = 0;
    virtual uint32_t height() const = 0;
    
    virtual uint32_t GetMipLevels() const = 0;
    virtual TextureFormat GetFormat() const = 0;
    virtual uint32_t GetFlags() const = 0;

    virtual uint32_t GetSampleCount() const = 0;
    virtual uint32_t GetSampleQuality() const = 0;

    bool IsFileImage() const { return file_image_; }

    virtual void ReleaseNativeResource() = 0;

    virtual bool Resize(uint32_t width, uint32_t height) = 0;

    virtual void ReadBackImage(CommandBuffer* cmd_buffer, int left, int top,
        int width, int height, int destX, int destY, ReadbackDelegate&& callback) = 0;

protected:
    bool file_image_ = false;
};

}
}
