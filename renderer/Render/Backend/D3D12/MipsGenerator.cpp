#include "MipsGenerator.h"
#include "d3dx12.h"
#include "Exception/Exception.h"
#include "Shader.h"
#include "GfxDriver.h"
#include "Render/Base/Util.h"
#include "Texture.h"
#include "Program.h"
#include "CommandBuffer.h"

namespace glacier {
namespace render {

MipsGenerator::MipsGenerator(ID3D12Device* device) :
    device(device),
    allocator_(device, D3D12_HEAP_TYPE_DEFAULT, D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES)
{
    CreateProgram();
    CreateDefaultUav();
}

void MipsGenerator::Generate(D3D12CommandBuffer* command_list, D3D12Texture* texture)
{
    D3D12_RESOURCE_DESC tex_desc = texture->GetDesc();
    std::shared_ptr<D3D12Texture> uav_res;
    std::shared_ptr<D3D12Texture> alias_res;

    D3D12Texture* uav_tex = texture;

    // If the passed-in resource does not allow for UAV access
    // then create a staging resource that is used to generate
    // the mipmap chain.
    if (!texture->CheckUAVSupport() || (tex_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0) {
        CreateUavResource(command_list, texture, uav_res, alias_res);
        uav_tex = uav_res.get();
    }

    program_->BindPSO(command_list);

    if (tex_desc.DepthOrArraySize > 1) {
        GenerateTextureArray(command_list, uav_tex);
    }
    else {
        GenerateTexture2D(command_list, uav_tex);
    }

    if (uav_res) {
        command_list->AliasResource(uav_tex, alias_res.get());
        command_list->CopyResource(alias_res.get(), texture);

        command_list->AddInflightResource(std::move(uav_res));
        command_list->AddInflightResource(std::move(alias_res));
    }
}

void MipsGenerator::GenerateTexture2D(D3D12CommandBuffer* command_list, D3D12Texture* texture) {
    auto gfx = D3D12GfxDriver::Instance();
    auto allocator = gfx->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    command_list->TransitionBarrier(texture, ResourceAccessBit::kShaderWrite);// D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    D3D12_RESOURCE_DESC tex_desc = texture->GetDesc();
    auto resource = static_cast<ID3D12Resource*>(texture->GetNativeResource());

    GenerateMipsCB cbuffer;
    cbuffer.IsSRGB = IsSRGBFormat(tex_desc.Format);
    
    //Prepare the shader resource view description for the source texture
    D3D12_SHADER_RESOURCE_VIEW_DESC src_srv_desc = {};
    src_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    src_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    src_srv_desc.Format = tex_desc.Format;
    src_srv_desc.Texture2D.MipLevels = tex_desc.MipLevels;
    src_srv_desc.Texture2D.MostDetailedMip = 0;

    auto src_slot = allocator->Allocate();
    D3D12_CPU_DESCRIPTOR_HANDLE src_srv = src_slot.GetDescriptorHandle();
    device->CreateShaderResourceView(resource, &src_srv_desc, src_srv);

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

        command_list->SetCompute32BitConstants(kCbuffer, cbuffer);
        command_list->SetDescriptorTable(kSrcMip, 0, &src_slot);

        command_list->TransitionBarrier(texture, ResourceAccessBit::kNonPixelShaderRead, srcMip);

        auto dst_slot = allocator->Allocate(4);
        D3D12_CPU_DESCRIPTOR_HANDLE uav_arr[4];

        for (uint32_t mip = 0; mip < mipCount; ++mip) {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = GetUAVCompatableFormat(tex_desc.Format);
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.MipSlice = srcMip + mip + 1;
            uav_arr[mip] = dst_slot.GetDescriptorHandle(mip);

            device->CreateUnorderedAccessView(resource, nullptr, &uavDesc, uav_arr[mip]);
        }

        // Pad any unused mip levels with a default UAV. Doing this keeps the DX12 runtime happy.
        if (mipCount < 4)
        {
            for (uint32_t i = mipCount; i < 4; ++i) {
                uav_arr[i] = uav_slot_.GetDescriptorHandle(i);
            }
        }

        command_list->SetDescriptorTable(kOutMip, 0, &dst_slot);

        command_list->Dispatch(math::DivideByMultiple(dstWidth, 8), math::DivideByMultiple(dstHeight, 8), 1);

        command_list->TransitionBarrier(texture, ResourceAccessBit::kShaderWrite, srcMip);

        // make sure write completed
        command_list->UavResource(resource);

        srcMip += mipCount;

        command_list->AddInflightResource(std::move(dst_slot));
    }

    command_list->AddInflightResource(std::move(src_slot));
}

void MipsGenerator::GenerateTextureArray(D3D12CommandBuffer* command_list, D3D12Texture* texture) {
    auto gfx = D3D12GfxDriver::Instance();
    auto allocator = gfx->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    //required by state tracker
    command_list->TransitionBarrier(texture, ResourceAccessBit::kShaderWrite);

    D3D12_RESOURCE_DESC tex_desc = texture->GetDesc();
    auto resource = static_cast<ID3D12Resource*>(texture->GetNativeResource());

    GenerateMipsCB cbuffer;
    cbuffer.IsSRGB = IsSRGBFormat(tex_desc.Format);

    //Prepare the shader resource view description for the source texture
    D3D12_SHADER_RESOURCE_VIEW_DESC src_srv_desc = {};
    src_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    src_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    src_srv_desc.Format = tex_desc.Format;
    src_srv_desc.Texture2DArray.ArraySize = 1;
    src_srv_desc.Texture2DArray.MipLevels = -1;
    src_srv_desc.Texture2DArray.MostDetailedMip = 0;

    for (int array_index = 0; array_index < tex_desc.DepthOrArraySize; ++array_index) {
        auto src_slot = allocator->Allocate();
        D3D12_CPU_DESCRIPTOR_HANDLE src_srv = src_slot.GetDescriptorHandle();

        src_srv_desc.Texture2DArray.FirstArraySlice = array_index;
        device->CreateShaderResourceView(resource, &src_srv_desc, src_srv);

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

            command_list->SetCompute32BitConstants(kCbuffer, cbuffer);
            command_list->SetDescriptorTable(kSrcMip, 0, &src_slot);

            command_list->TransitionBarrier(texture, ResourceAccessBit::kNonPixelShaderRead,
                array_index * tex_desc.MipLevels + srcMip);

            auto dst_slot = allocator->Allocate(4);
            D3D12_CPU_DESCRIPTOR_HANDLE uav_arr[4];

            for (uint32_t mip = 0; mip < mipCount; ++mip) {
                D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                uavDesc.Format = GetUAVCompatableFormat(tex_desc.Format);
                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                uavDesc.Texture2DArray.ArraySize = 1;
                uavDesc.Texture2DArray.MipSlice = srcMip + mip + 1;
                uavDesc.Texture2DArray.FirstArraySlice = array_index;
                uav_arr[mip] = dst_slot.GetDescriptorHandle(mip);

                device->CreateUnorderedAccessView(resource, nullptr, &uavDesc, uav_arr[mip]);
            }

            // Pad any unused mip levels with a default UAV. Doing this keeps the DX12 runtime happy.
            if (mipCount < 4)
            {
                for (uint32_t i = mipCount; i < 4; ++i) {
                    uav_arr[i] = uav_slot_.GetDescriptorHandle(i);
                }
            }

            command_list->SetDescriptorTable(kOutMip, 0, &dst_slot);

            command_list->Dispatch(math::DivideByMultiple(dstWidth, 8), math::DivideByMultiple(dstHeight, 8), 1);

            command_list->TransitionBarrier(texture, ResourceAccessBit::kShaderWrite,
                array_index * tex_desc.MipLevels + srcMip);

            // make sure write completed
            command_list->UavResource(resource);

            srcMip += mipCount;

            command_list->AddInflightResource(std::move(dst_slot));
        }
        command_list->AddInflightResource(std::move(src_slot));
    }
}

void MipsGenerator::CreateUavResource(D3D12CommandBuffer* cmd_list, D3D12Texture* texture,
    std::shared_ptr<D3D12Texture>& uav_res, std::shared_ptr<D3D12Texture>& alias_res)
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

    // Create a placed resource that matches the description of the original resource.
    // This resource is used to copy the original texture to the UAV compatible resource.
    auto alias_location = allocator_.AllocResource((size_t)allocationInfo.SizeInBytes, allocationInfo.Alignment,
        aliasDesc, D3D12_RESOURCE_STATE_COMMON);

    alias_res = std::make_shared<D3D12Texture>(alias_location.GetResource());
    alias_res->SetName("Alias Resource for MipsGenerate");

    // Create a UAV compatible resource in the same heap as the alias resource.
    auto native_alias_res = alias_location.CreateAliasResource(uavDesc, alias_location.GetState());
    uav_res = std::make_shared<D3D12Texture>(
        native_alias_res,
        alias_location.GetState()
    );

    uav_res->SetName("UAV Texture for MipsGenerate");

    // Add an aliasing barrier for the alias resource.
    cmd_list->AliasResource(nullptr, alias_res.get());

    // Copy the original resource to the alias resource.
    // This ensures GPU validation.
    cmd_list->CopyResource(texture, alias_res.get());

    // Add an aliasing barrier for the UAV compatible resource.
    cmd_list->AliasResource(alias_res.get(), uav_res.get());

    cmd_list->AddInflightResource(std::move(alias_location));
}

void MipsGenerator::CreateProgram() {
    program_ = std::make_unique<D3D12Program>("GenerateMips");
    auto gfx = D3D12GfxDriver::Instance();
    auto shader = gfx->CreateShader(ShaderType::kCompute, L"GenerateMipsCS", "main");

    program_->SetShader(shader);
    program_->SetRootConstants("GenerateMipsCB", sizeof(GenerateMipsCB) / sizeof(uint32_t));

    CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    program_->SetStaticSampler("LinearClampSampler", linearClampSampler);
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
