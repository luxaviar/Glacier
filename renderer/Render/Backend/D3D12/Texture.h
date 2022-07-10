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
class CommandBuffer;

class D3D12Texture : public Texture {
public:
    struct ReadbackTask {
        uint64_t raw_pitch;
        ResourceLocation stage_location;
        ReadbackDelegate callback;

        void Process();
    };

    static uint32_t CalcSubresource(uint32_t mip_slice, uint32_t array_slice, uint32_t mip_levels) {
        return mip_slice + array_slice * mip_levels;
    }

    D3D12Texture(CommandBuffer* cmd_buffer, const Image& image, bool gen_mips, TextureType type = TextureType::kTexture2D);
    D3D12Texture(const ComPtr<ID3D12Resource>& res, D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON);
    D3D12Texture(const TextureDescription& desc, const D3D12_CLEAR_VALUE* clear_value = nullptr);

    bool CheckFormatSupport(D3D12_FORMAT_SUPPORT1 formatSupport) const;
    bool CheckFormatSupport(D3D12_FORMAT_SUPPORT2 formatSupport) const;
    bool CheckUAVSupport() const;

    void SetName(const char* name) override;
    D3D12_RESOURCE_DESC GetDesc() { return desc_; }

    uint32_t width() const override { return (uint32_t)desc_.Width; }
    uint32_t height() const override { return (uint32_t)desc_.Height; }
    TextureFormat GetFormat() const override { return GetTextureFormat(desc_.Format); }
    DXGI_FORMAT GetNativeFormat() const { return desc_.Format; }

    uint32_t GetMipLevels() const override { return desc_.MipLevels; }
    uint32_t GetFlags() const override { return desc_.Flags; }

    uint32_t GetSampleCount() const override { return desc_.SampleDesc.Count; }
    uint32_t GetSampleQuality() const override { return desc_.SampleDesc.Quality; }

    D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const { return gpu_address_; }
    const D3D12DescriptorRange& GetDescriptorSlot() const { return descriptor_slot_; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle() const { return descriptor_slot_.GetDescriptorHandle(); }

    void Reset(ComPtr<ID3D12Resource>& res, D3D12_RESOURCE_STATES state);
    bool Resize(uint32_t width, uint32_t height) override;

    void ReleaseNativeResource() override;
    void* GetNativeResource() const override { return resource_.Get(); }

    void ReadBackImage(CommandBuffer* cmd_buffer, int left, int top,
        int width, int height, int destX, int destY, ReadbackDelegate&& callback) override;

protected:
    void Initialize(const ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES state);
    void Initialize(const ResourceLocation& resource_location);

    uint8_t CalculatePlantCount() const;
    uint32_t CalculateNumSubresources() const;
    void CheckFeatureSupport();

    void Create(const TextureDescription& desc, const D3D12_CLEAR_VALUE* clear_value);
    void Create(const D3D12_RESOURCE_DESC& desc, const D3D12_CLEAR_VALUE* clear_value);

    void CreateTextureFromImage(CommandBuffer* cmd_buffer, const Image& image, bool gen_mips);
    void CreateCubeMapFromImage(CommandBuffer* cmd_buffer, const Image& image, bool gen_mips);

    void UploadTexture(CommandBuffer* cmd_buffer, const std::vector<D3D12_SUBRESOURCE_DATA>& subresources);

    void CreateViews();

    bool force_srv_ = false;
    bool cube_map_ = false;

    bool clear_value_set_ = false;
    D3D12_CLEAR_VALUE clear_value_;

    ResourceLocation location_;
    ComPtr<ID3D12Resource> resource_;
    D3D12_RESOURCE_DESC desc_ = {};
    uint8_t plant_count_ = 0;

    D3D12_GPU_VIRTUAL_ADDRESS gpu_address_ = 0;
    D3D12DescriptorRange descriptor_slot_ = {};

    D3D12_FEATURE_DATA_FORMAT_SUPPORT format_support_;
};

}
}
