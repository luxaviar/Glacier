#pragma once

#include <d3d11_1.h>
#include "Resource.h"
#include "render/base/texture.h"

namespace glacier {
namespace render {

class SwapChain;

class D3D11Texture : public Texture, public D3D11Resource {
public:
    D3D11Texture(const TextureDescription& desc);
    D3D11Texture(SwapChain* swapchain);

    ID3D11Resource* GetUnderlyingResource() const override { return texture_.Get();}

    void Reset(const ComPtr<ID3D11Texture2D>& res);

    uint32_t GetMipLevels() const override;

    void Bind(ShaderType shader_type, uint16_t slot);
    void UnBind(ShaderType shader_type, uint16_t slot);

    void GenerateMipMaps() override;

    void ReleaseUnderlyingResource() override;
    bool Resize(uint32_t width, uint32_t height) override;

    void* underlying_resource() const override { return texture_.Get(); }

    void ReadBackImage(int left, int top,
        int width, int height, int destX, int destY, ReadbackDelegate&& callback);

private:
    void CreateFromFile();
    void CreateFromColor();
    void Create();

    void CreateViews(const D3D11_TEXTURE2D_DESC& desc);

    void CreateTextureFromImage(const Image& image, ID3D11Texture2D** tex, ID3D11ShaderResourceView** srv);
    void CreateCubeMapFromImage(const Image& image);

    ComPtr<ID3D11Texture2D> texture_;
    ComPtr<ID3D11ShaderResourceView> srv_;

    //TODO: add uav creation
    ComPtr<ID3D11UnorderedAccessView> uav_;
};

}
}
