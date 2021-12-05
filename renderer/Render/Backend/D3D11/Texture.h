#pragma once

#include <d3d11_1.h>
#include "render/base/texture.h"

namespace glacier {
namespace render {

class D3D11Texture : public Texture {
public:
    D3D11Texture(const TextureBuilder& builder);

    uint32_t GetMipLevels() const override;

    void Bind(ShaderType shader_type, uint16_t slot) override;
    void UnBind(ShaderType shader_type, uint16_t slot) override;

    void GenerateMipMaps() override;

    void ReleaseUnderlyingResource() override;
    bool Resize(uint32_t width, uint32_t height) override;
    bool RefreshBackBuffer() override;

    void* underlying_resource() const override { return texture_.Get(); }

    bool ReadBackImage(Image& image, int left, int top,
        int width, int height, int destX, int destY, const ReadBackResolver& resolver) const override;

    bool ReadBackImage(Image& image, int left, int top,
        int width, int height, int destX, int destY) const override;

private:
    void CreateFromBackBuffer();
    void CreateFromFile();
    void CreateFromColor();
    void Create();

    void CreateTextureFromImage(const Image& image, ID3D11Texture2D** tex, ID3D11ShaderResourceView** srv);
    void CreateCubeMapFromImage(const Image& image);

    ComPtr<ID3D11Texture2D> texture_;
    ComPtr<ID3D11ShaderResourceView> srv_;

    //TODO: add uav creation
    ComPtr<ID3D11UnorderedAccessView> uav_;
};

}
}
