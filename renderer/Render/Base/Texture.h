#pragma once

#include <algorithm>
#include <functional>
#include "resource.h"
#include "common/color.h"
#include "Common/Util.h"

namespace glacier {
namespace render {

class Image;

struct TextureBuilder {
    TextureBuilder& EnableSRGB(bool v = true) { srgb = v; return *this; }
    TextureBuilder& EnableMips(bool v = true) { gen_mips = v; return *this; }
    TextureBuilder& SetFormat(TextureFormat fmt) { format = fmt; return *this; }
    //TextureBuilder& SetBindFlag(uint32_t flags) { bind_flags = flags; return *this; }
    TextureBuilder& SetBackBufferTexture(int idx) { backbuffer_index = idx; return *this; }
    TextureBuilder& SetFile(const TCHAR* file_) { file = file_; return *this; }
    TextureBuilder& SetDimension(uint32_t w, uint32_t h) { width = w; height = h; return *this; }
    TextureBuilder& SetColor(const Color& color_) { color = color_; use_color = true; return *this; }
    TextureBuilder& SetSampleDesc(uint32_t count, uint32_t qulity) { sample_count = count; sample_quality = qulity; return *this; }
    TextureBuilder& SetType(TextureType type_) {
        type = type_;
        if (type == TextureType::kTextureCube) {
            depth_or_array_size = 6;
        }

        return *this;
    }
    TextureBuilder& SetCreateFlag(uint32_t flags) { create_flags = flags; return *this; }

    bool srgb = false;
    bool gen_mips = false;
    TextureFormat format = TextureFormat::kR8G8B8A8_UNORM;
    //uint32_t bind_flags = 0;
    uint32_t create_flags = 0;
    TextureType type = TextureType::kTexture2D;

    int backbuffer_index = -1;
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
    static TextureBuilder Builder() { return TextureBuilder{}; }

    Texture(const TextureBuilder& builder) : 
        detail_(builder)
    {}

    uint32_t width() const { return detail_.width; }
    uint32_t height() const { return detail_.height; }
    virtual uint32_t GetMipLevels() const = 0;
    bool IsCubeMap() const { return detail_.type == TextureType::kTextureCube; }

    virtual void Bind(ShaderType shader_type, uint16_t slot) = 0;
    virtual void UnBind(ShaderType shader_type, uint16_t slot) = 0;

    virtual void GenerateMipMaps() = 0;

    virtual void ReleaseUnderlyingResource() = 0;
    virtual bool Resize(uint32_t width, uint32_t height) = 0;
    virtual bool RefreshBackBuffer() = 0;

    virtual bool ReadBackImage(Image& image, int left, int top,
        int width, int height, int destX, int destY, const ReadBackResolver& resolver) const = 0;

    virtual bool ReadBackImage(Image& image, int left, int top,
        int width, int height, int destX, int destY) const = 0;

protected:
    static uint32_t CalculateNumberOfMipLevels(uint32_t width, uint32_t height) noexcept {
        return (uint32_t)std::floor(std::log2(std::max(width, height))) + 1;
    }

    TextureBuilder detail_;
};

using TextureGurad = ResourceGuardEx<Texture>;

}
}
