#include "MipsGenerator.h"
#include "d3dx12.h"
#include "Exception/Exception.h"
#include "Shader.h"
#include "GfxDriver.h"
#include "Render/Base/Util.h"
#include "Texture.h"

namespace glacier {
namespace render {

MipsGenerator::MipsGenerator(ID3D12Device* device) :
    device(device),
    allocator_(device, D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES)
{
    CreateRootSignature();
    CreatePSO();
    CreateDefaultUav();
}

void MipsGenerator::Generate(D3D12CommandList* command_list, D3D12Texture* texture)
{

    D3D12_RESOURCE_DESC tex_desc = texture->GetDesc();
    ResourceLocation uav_location;
    ResourceLocation alias_location;

    ID3D12Resource* uav_tex = texture->GetUnderlyingResource().Get();
    D3D12_RESOURCE_STATES uav_state = texture->GetState();

    // If the passed-in resource does not allow for UAV access
    // then create a staging resource that is used to generate
    // the mipmap chain.
    if (!texture->CheckUAVSupport() || (tex_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0) {
        CreateUavResource(command_list, texture, uav_location, alias_location);
        uav_tex = uav_location.resource.Get();
        uav_state = D3D12_RESOURCE_STATE_COMMON;
    }

    if (tex_desc.DepthOrArraySize > 1) {
        GenerateTextureArray(command_list, uav_tex, uav_state);
    }
    else {
        GenerateTexture2D(command_list, uav_tex, uav_state);
    }

    if (uav_location.resource) {
        auto gfx = D3D12GfxDriver::Instance();
        command_list->AliasResource(uav_tex, alias_location.resource.Get());
        command_list->CopyResource(alias_location.resource.Get(), D3D12_RESOURCE_STATE_COMMON,
            texture->GetUnderlyingResource().Get(), texture->GetState());

        // Make sure the heap does not go out of scope until the command list
        // is finished executing on the command queue.
        gfx->AddInflightResource(std::move(uav_location));
        gfx->AddInflightResource(std::move(alias_location));
    }
}

void MipsGenerator::GenerateTexture2D(D3D12CommandList* command_list,
    ID3D12Resource* texture, D3D12_RESOURCE_STATES state)
{
    //Set root signature, pso and descriptor heap
    command_list->SetComputeRootSignature(root_signature.Get());
    command_list->SetPipelineState(pso.Get());

    auto gfx = D3D12GfxDriver::Instance();
    auto allocator = gfx->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto desc_tb = gfx->GetSrvUavTableHeap();

    command_list->TransitionBarrier(texture, state, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    D3D12_RESOURCE_DESC tex_desc = texture->GetDesc();

    GenerateMipsCB cbuffer;
    cbuffer.IsSRGB = IsSRGBFormat(tex_desc.Format);
    
    //Prepare the shader resource view description for the source texture
    D3D12_SHADER_RESOURCE_VIEW_DESC src_srv_desc = {};
    src_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    src_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    src_srv_desc.Format = tex_desc.Format;// cbuffer.IsSRGB ? GetSRGBFormat(tex_desc.Format) : tex_desc.Format;
    src_srv_desc.Texture2D.MipLevels = tex_desc.MipLevels;
    src_srv_desc.Texture2D.MostDetailedMip = 0;

    auto src_slot = allocator->Allocate();
    D3D12_CPU_DESCRIPTOR_HANDLE src_srv = src_slot.GetDescriptorHandle();
    device->CreateShaderResourceView(texture, &src_srv_desc, src_srv);

    auto src_gpu = desc_tb->AppendDescriptors(&src_srv, 1);

    for (uint32_t srcMip = 0; srcMip < tex_desc.MipLevels - 1u; ) {
        uint64_t srcWidth = tex_desc.Width >> srcMip;
        uint32_t srcHeight = tex_desc.Height >> srcMip;
        uint32_t dstWidth = static_cast<uint32_t>(srcWidth >> 1);
        uint32_t dstHeight = srcHeight >> 1;

        // 0b00(0): Both width and height are even.
        // 0b01(1): Width is odd, height is even.
        // 0b10(2): Width is even, height is odd.
        // 0b11(3): Both width and height are odd.
        cbuffer.SrcDimension = (srcHeight & 1) << 1 | (srcWidth & 1);

        // How many mipmap levels to compute this pass (max 4 mips per pass)
        DWORD mipCount;

        // The number of times we can half the size of the texture and get
        // exactly a 50% reduction in size.
        // A 1 bit in the width or height indicates an odd dimension.
        // The case where either the width or the height is exactly 1 is handled
        // as a special case (as the dimension does not require reduction).
        _BitScanForward(&mipCount,
            (dstWidth == 1 ? dstHeight : dstWidth) | (dstHeight == 1 ? dstWidth : dstHeight));
        // Maximum number of mips to generate is 4.
        mipCount = std::min<DWORD>(4, mipCount + 1);
        // Clamp to total number of mips left over.
        mipCount = (srcMip + mipCount) >= tex_desc.MipLevels ? tex_desc.MipLevels - srcMip - 1 : mipCount;

        // Dimensions should not reduce to 0.
        // This can happen if the width and height are not the same.
        dstWidth = std::max<DWORD>(1, dstWidth);
        dstHeight = std::max<DWORD>(1, dstHeight);

        cbuffer.SrcMipLevel = srcMip;
        cbuffer.NumMipLevels = mipCount;
        cbuffer.TexelSize.x = 1.0f / (float)dstWidth;
        cbuffer.TexelSize.y = 1.0f / (float)dstHeight;

        command_list->SetComputeRoot32BitConstants(kCbuffer, sizeof(cbuffer) / sizeof(uint32_t), &cbuffer, 0);
        command_list->SetComputeRootDescriptorTable(kSrcMip, src_gpu);

        auto dst_slot = allocator->Allocate(4);
        D3D12_CPU_DESCRIPTOR_HANDLE uav_arr[4];

        for (uint32_t mip = 0; mip < mipCount; ++mip) {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = GetUAVCompatableFormat(tex_desc.Format);
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.MipSlice = srcMip + mip + 1;
            uav_arr[mip] = dst_slot.GetDescriptorHandle(mip);

            device->CreateUnorderedAccessView(texture, nullptr, &uavDesc, uav_arr[mip]);
        }

        // Pad any unused mip levels with a default UAV. Doing this keeps the DX12 runtime happy.
        if (mipCount < 4)
        {
            for (uint32_t i = mipCount; i < 4; ++i) {
                uav_arr[i] = uav_slot_.GetDescriptorHandle(i);
            }
        }

        auto dst_gpu = desc_tb->AppendDescriptors(uav_arr, 4);
        command_list->SetComputeRootDescriptorTable(kOutMip, dst_gpu);

        command_list->Dispatch(math::DivideByMultiple(dstWidth, 8), math::DivideByMultiple(dstHeight, 8), 1);

        // make sure write completed
        command_list->UavResource(texture);

        srcMip += mipCount;

        gfx->AddInflightResource(std::move(dst_slot));
    }

    command_list->TransitionBarrier(texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, state);

    gfx->AddInflightResource(std::move(src_slot));
}

void MipsGenerator::GenerateTextureArray(D3D12CommandList* command_list,
    ID3D12Resource* texture, D3D12_RESOURCE_STATES state)
{
    //Set root signature, pso and descriptor heap
    command_list->SetComputeRootSignature(root_signature.Get());
    command_list->SetPipelineState(pso.Get());

    auto gfx = D3D12GfxDriver::Instance();
    auto allocator = gfx->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto desc_tb = gfx->GetSrvUavTableHeap();

    command_list->TransitionBarrier(texture, state, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    D3D12_RESOURCE_DESC tex_desc = texture->GetDesc();

    GenerateMipsCB cbuffer;
    cbuffer.IsSRGB = IsSRGBFormat(tex_desc.Format);

    //Prepare the shader resource view description for the source texture
    D3D12_SHADER_RESOURCE_VIEW_DESC src_srv_desc = {};
    src_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    src_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    src_srv_desc.Format = tex_desc.Format;// GetUAVCompatableFormat(tex_desc.Format);
    src_srv_desc.Texture2DArray.ArraySize = 1;
    src_srv_desc.Texture2DArray.MipLevels = tex_desc.MipLevels;
    src_srv_desc.Texture2DArray.MostDetailedMip = 0;

    for (int i = 0; i < tex_desc.DepthOrArraySize; ++i) {
        auto src_slot = allocator->Allocate();
        D3D12_CPU_DESCRIPTOR_HANDLE src_srv = src_slot.GetDescriptorHandle();

        src_srv_desc.Texture2DArray.FirstArraySlice = i;
        device->CreateShaderResourceView(texture, &src_srv_desc, src_srv);

        auto src_gpu = desc_tb->AppendDescriptors(&src_srv, 1);

        for (uint32_t srcMip = 0; srcMip < tex_desc.MipLevels - 1u; ) {
            uint64_t srcWidth = tex_desc.Width >> srcMip;
            uint32_t srcHeight = tex_desc.Height >> srcMip;
            uint32_t dstWidth = static_cast<uint32_t>(srcWidth >> 1);
            uint32_t dstHeight = srcHeight >> 1;

            // 0b00(0): Both width and height are even.
            // 0b01(1): Width is odd, height is even.
            // 0b10(2): Width is even, height is odd.
            // 0b11(3): Both width and height are odd.
            cbuffer.SrcDimension = (srcHeight & 1) << 1 | (srcWidth & 1);

            // How many mipmap levels to compute this pass (max 4 mips per pass)
            DWORD mipCount;

            // The number of times we can half the size of the texture and get
            // exactly a 50% reduction in size.
            // A 1 bit in the width or height indicates an odd dimension.
            // The case where either the width or the height is exactly 1 is handled
            // as a special case (as the dimension does not require reduction).
            _BitScanForward(&mipCount,
                (dstWidth == 1 ? dstHeight : dstWidth) | (dstHeight == 1 ? dstWidth : dstHeight));
            // Maximum number of mips to generate is 4.
            mipCount = std::min<DWORD>(4, mipCount + 1);
            // Clamp to total number of mips left over.
            mipCount = (srcMip + mipCount) >= tex_desc.MipLevels ? tex_desc.MipLevels - srcMip - 1 : mipCount;

            // Dimensions should not reduce to 0.
            // This can happen if the width and height are not the same.
            dstWidth = std::max<DWORD>(1, dstWidth);
            dstHeight = std::max<DWORD>(1, dstHeight);

            cbuffer.SrcMipLevel = srcMip;
            cbuffer.NumMipLevels = mipCount;
            cbuffer.TexelSize.x = 1.0f / (float)dstWidth;
            cbuffer.TexelSize.y = 1.0f / (float)dstHeight;

            command_list->SetComputeRoot32BitConstants(kCbuffer, sizeof(cbuffer) / sizeof(uint32_t), &cbuffer, 0);
            command_list->SetComputeRootDescriptorTable(kSrcMip, src_gpu);

            auto dst_slot = allocator->Allocate(4);
            D3D12_CPU_DESCRIPTOR_HANDLE uav_arr[4];

            for (uint32_t mip = 0; mip < mipCount; ++mip) {
                D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                uavDesc.Format = GetUAVCompatableFormat(tex_desc.Format);
                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                uavDesc.Texture2DArray.ArraySize = 1;
                uavDesc.Texture2DArray.MipSlice = srcMip + mip + 1;
                uavDesc.Texture2DArray.FirstArraySlice = i;
                uav_arr[mip] = dst_slot.GetDescriptorHandle(mip);

                device->CreateUnorderedAccessView(texture, nullptr, &uavDesc, uav_arr[mip]);
            }

            // Pad any unused mip levels with a default UAV. Doing this keeps the DX12 runtime happy.
            if (mipCount < 4)
            {
                for (uint32_t i = mipCount; i < 4; ++i) {
                    uav_arr[i] = uav_slot_.GetDescriptorHandle(i);
                }
            }

            auto dst_gpu = desc_tb->AppendDescriptors(uav_arr, 4);
            command_list->SetComputeRootDescriptorTable(kOutMip, dst_gpu);

            command_list->Dispatch(math::DivideByMultiple(dstWidth, 8), math::DivideByMultiple(dstHeight, 8), 1);

            // make sure write completed
            command_list->UavResource(texture);

            srcMip += mipCount;

            gfx->AddInflightResource(std::move(dst_slot));
        }

        gfx->AddInflightResource(std::move(src_slot));
    }

    command_list->TransitionBarrier(texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, state);
}

void MipsGenerator::CreateUavResource(D3D12CommandList* cmd_list, D3D12Texture* texture, ResourceLocation& uav_location, ResourceLocation& alias_location)
{
    D3D12_RESOURCE_DESC desc = texture->GetDesc();
    auto gfx = D3D12GfxDriver::Instance();
    auto device = gfx->GetDevice();

    // Describe an alias resource that is used to copy the original texture.
    auto aliasDesc = desc;
    // Placed resources can't be render targets or depth-stencil views.
    aliasDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    aliasDesc.Flags &= ~(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    // Describe a UAV compatible resource that is used to perform
    // mipmapping of the original texture.
    auto uavDesc = aliasDesc;  // The flags for the UAV description must match that of the alias description.
    uavDesc.Format = GetUAVCompatableFormat(desc.Format);

    D3D12_RESOURCE_DESC resourceDescs[] = { aliasDesc, uavDesc };

    // Create a heap that is large enough to store a copy of the original resource.
    auto allocationInfo = device->GetResourceAllocationInfo(0, _countof(resourceDescs), resourceDescs);

    alias_location = allocator_.AllocResource((size_t)allocationInfo.SizeInBytes, allocationInfo.Alignment);

    // Create placed resource
    auto allocator = static_cast<UavHeapAllocator::AllocatorPool::AllocatorType*>(alias_location.block.allocator);
    auto& heap = allocator->GetUnderlyingResource();

    uint64_t offset = alias_location.block.align_offset;

    ComPtr<ID3D12Resource> alias_res;
    // Create a placed resource that matches the description of the
    // original resource. This resource is used to copy the original
    // texture to the UAV compatible resource.
    GfxThrowIfFailed(device->CreatePlacedResource(heap.Get(), offset, &aliasDesc, D3D12_RESOURCE_STATE_COMMON,
        nullptr, IID_PPV_ARGS(&alias_res)));
    alias_location.resource = alias_res;

    // Create a UAV compatible resource in the same heap as the alias
    // resource.
    ComPtr<ID3D12Resource> uav_res;
    GfxThrowIfFailed(device->CreatePlacedResource(heap.Get(), offset, &uavDesc, D3D12_RESOURCE_STATE_COMMON,
        nullptr, IID_PPV_ARGS(&uav_res)));

    uav_location = alias_location.GetAliasLocation();
    uav_location.resource = uav_res;

    // Add an aliasing barrier for the alias resource.
    cmd_list->AliasResource(nullptr, alias_res.Get());

    // Copy the original resource to the alias resource.
    // This ensures GPU validation.
    cmd_list->CopyResource(texture->GetUnderlyingResource().Get(), texture->GetState(),
        alias_res.Get(), D3D12_RESOURCE_STATE_COMMON);

    // Add an aliasing barrier for the UAV compatible resource.
    cmd_list->AliasResource(alias_res.Get(), uav_res.Get());
}

void MipsGenerator::CreateRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE1 srcMip(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0,
        D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);
    CD3DX12_DESCRIPTOR_RANGE1 outMip(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0, 0,
        D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE);

    CD3DX12_ROOT_PARAMETER1 rootParameters[kNumRootParameters];
    rootParameters[kCbuffer].InitAsConstants(sizeof(GenerateMipsCB) / 4, 0);
    rootParameters[kSrcMip].InitAsDescriptorTable(1, &srcMip);
    rootParameters[kOutMip].InitAsDescriptorTable(1, &outMip);

    CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc(kNumRootParameters, rootParameters, 1,
        &linearClampSampler);

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data = {};
    feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data))))
    {
        feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }
    GfxThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, feature_data.HighestVersion, &signature, &error));
    GfxThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
}

void MipsGenerator::CreatePSO()
{
    auto cs_shader = std::make_shared<D3D12Shader>(ShaderType::kCompute, L"GenerateMipsCS", "main", "cs_5_1");
    D3D12_SHADER_BYTECODE cs{ cs_shader->GetBytecode()->GetBufferPointer(), cs_shader->GetBytecode()->GetBufferSize() };

    D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
    pso_desc.pRootSignature = root_signature.Get();
    pso_desc.CS = cs;
    device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso));
}

void MipsGenerator::CreateDefaultUav() {
    auto gfx = D3D12GfxDriver::Instance();
    auto allocator = gfx->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    uav_slot_ = allocator->Allocate(4);

    for (UINT i = 0; i < 4; ++i) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        uavDesc.Texture2D.MipSlice = i;
        uavDesc.Texture2D.PlaneSlice = 0;

        device->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, uav_slot_.GetDescriptorHandle(i));
    }
}

}
}
