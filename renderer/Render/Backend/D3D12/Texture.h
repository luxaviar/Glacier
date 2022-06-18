#pragma once

#include <d3d12.h>
#include <string>
#include "Common/Uncopyable.h"
#include "Common/Util.h"
#include "Resource.h"
#include "Util.h"
#include "Render/Base/Texture.h"
#include "DescriptorHeapAllocator.h"

namespace glacier {
namespace render {

class SwapChain;

class D3D12Texture : public D3D12Resource, public Texture {
public:
    struct ReadbackTask {
        uint64_t raw_pitch;
        ComPtr<ID3D12Resource> staging;
        ReadbackDelegate callback;

        void Process();
    };

    static uint32_t CalcSubresource(uint32_t mip_slice, uint32_t array_slice, uint32_t mip_levels) {
        return mip_slice + array_slice * mip_levels;
    }

    D3D12Texture(const TextureDescription& desc, const D3D12_CLEAR_VALUE* clear_value = nullptr);
    D3D12Texture(ComPtr<ID3D12Resource>& res, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON);
    D3D12Texture(const Image& image, D3D12_RESOURCE_STATES state, bool gen_mips);
    D3D12Texture(SwapChain* swapchain);

    void SetName(const TCHAR* name) override;
    const TCHAR* GetName(const TCHAR* name) const override;

    void Reset(ComPtr<ID3D12Resource>& res, D3D12_RESOURCE_STATES state);
    bool Resize(uint32_t width, uint32_t height) override;

    //const D3D12DescriptorRange& GetSrvDescriptorSlot() const { return srv_slot_; }
    //const D3D12DescriptorRange& GetUavDescriptorSlot() const { return uav_slot_; }

    //D3D12_CPU_DESCRIPTOR_HANDLE GetSrvDescriptorHandle() const override { return srv_slot_.GetDescriptorHandle(); }
    //D3D12_CPU_DESCRIPTOR_HANDLE GetUavDescriptorHandle(uint32_t miplevel=0) const override { return uav_slot_.GetDescriptorHandle(miplevel); }

    void GenerateMipMaps() override;
    uint32_t GetMipLevels() const override;

    void ReleaseUnderlyingResource() override;
    
    void ReadBackImage(int left, int top,
        int width, int height, int destX, int destY, ReadbackDelegate&& callback) override;

protected:
    void CreateFromFile();
    void CreateFromColor();
    void Create(const D3D12_CLEAR_VALUE* clear_value);

    void CreateTextureFromImage(const Image& image, D3D12_RESOURCE_STATES state, bool gen_mips);
    void CreateCubeMapFromImage(const Image& image);

    void UploadTexture(D3D12_RESOURCE_STATES state, const std::vector<D3D12_SUBRESOURCE_DATA>& subresources);

    void CreateViews();

    ResourceLocation location_;

    //D3D12DescriptorRange srv_slot_;
    //D3D12DescriptorRange uav_slot_;

    bool clear_value_set_ = false;
    D3D12_CLEAR_VALUE clear_value_;
};

}
}
