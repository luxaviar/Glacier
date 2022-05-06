#pragma once

#include <vector>
#include <d3d12.h>
#include "DescriptorHeapAllocator.h"
#include "MemoryHeapAllocator.h"

namespace glacier {
namespace render {

class D3D12Texture;

class MipsGenerator {
public:
    enum
    {
        kCbuffer,
        kSrcMip,
        kOutMip,
        kNumRootParameters
    };

    struct alignas(16) GenerateMipsCB {
        uint32_t          SrcMipLevel;   // Texture level of source mip
        uint32_t          NumMipLevels;  // Number of OutMips to write: [1-4]
        uint32_t          SrcDimension;  // Width and height of the source texture are even or odd.
        uint32_t          IsSRGB;        // Must apply gamma correction to sRGB textures.
        DirectX::XMFLOAT2 TexelSize;     // 1.0 / OutMip1.Dimensions
    };

    using UavHeapAllocator = D3D12PlacedHeapAllocator<next_log_of2(DEFAULT_DEFAULT_HEAP_SIZE), next_log_of2(DEFAULT_RESOURCE_ALIGNMENT)>;

    MipsGenerator(ID3D12Device* device);

    void Generate(D3D12CommandList* command_list, D3D12Resource* texture);

private:
    void GenerateTexture2D(D3D12CommandList* command_list,
        D3D12Resource* texture);// , D3D12_RESOURCE_STATES state);

    void GenerateTextureArray(D3D12CommandList* command_list,
        D3D12Resource* texture);// , D3D12_RESOURCE_STATES state);

    void CreateUavResource(D3D12CommandList* command_list,
        D3D12Resource* texture, std::shared_ptr<D3D12Resource>& uav_res, std::shared_ptr<D3D12Resource>& alias_res);

    void CreateRootSignature();
    void CreatePSO();
    void CreateDefaultUav();

    ID3D12Device* device;
    ComPtr<ID3D12PipelineState> pso;
    ComPtr<ID3D12RootSignature> root_signature;

    D3D12DescriptorRange uav_slot_;
    UavHeapAllocator allocator_;
};

}
}