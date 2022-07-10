#include "Texture.h"
#include <DirectXHelpers.h>
#include <array>
#include "Exception/Exception.h"
#include "GfxDriver.h"
#include "render/image.h"
#include "math/Vec2.h"
#include "Common/ScopedBuffer.h"
#include "CommandBuffer.h"

namespace glacier {
namespace render {

D3D12Texture::D3D12Texture(const TextureDescription& desc, const D3D12_CLEAR_VALUE* clear_value)
{
    if (clear_value) {
        clear_value_ = *clear_value;
        clear_value_set_ = true;
    }

    force_srv_ = (desc.create_flags & (uint32_t)CreateFlags::kShaderResource) != 0;
    cube_map_ = desc.type == TextureType::kTextureCube;

    Create(desc, clear_value);

    CheckFeatureSupport();
    CreateViews();
}

D3D12Texture::D3D12Texture(const ComPtr<ID3D12Resource>& res, D3D12_RESOURCE_STATES state) :
    resource_(res),
    desc_(res->GetDesc()),
    plant_count_(CalculatePlantCount())
{
    state_ = { CalculateNumSubresources(), (ResourceAccessBit)state };

    CheckFeatureSupport();
    CreateViews();
}

D3D12Texture::D3D12Texture(CommandBuffer* cmd_buffer, const Image& image, bool gen_mips, TextureType type)
{
    file_image_ = true;

    if (type == TextureType::kTextureCube) {
        CreateCubeMapFromImage(cmd_buffer, image, gen_mips);
    }
    else {
        CreateTextureFromImage(cmd_buffer, image, gen_mips);
    }

    CheckFeatureSupport();
    CreateViews();
}

void D3D12Texture::Initialize(const ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES state) {
    resource_ = resource;
    desc_ = resource->GetDesc();
    plant_count_ = CalculatePlantCount();
    state_ = { CalculateNumSubresources(), (ResourceAccessBit)state };
}

void D3D12Texture::Initialize(const ResourceLocation& resource_location) {
    Initialize(resource_location.GetResource(), resource_location.GetState());

    gpu_address_ = resource_location.GetGpuAddress();
}

void D3D12Texture::Reset(ComPtr<ID3D12Resource>& res, D3D12_RESOURCE_STATES state) {
    location_ = {};
    Initialize(res, state);
}

void D3D12Texture::SetName(const char* name) {
    Texture::SetName(name);
    resource_->SetName(ToWide(name).c_str());
}

bool D3D12Texture::CheckFormatSupport(D3D12_FORMAT_SUPPORT1 formatSupport) const
{
    return (format_support_.Support1 & formatSupport) != 0;
}

bool D3D12Texture::CheckFormatSupport(D3D12_FORMAT_SUPPORT2 formatSupport) const
{
    return (format_support_.Support2 & formatSupport) != 0;
}

bool D3D12Texture::CheckUAVSupport() const
{
    return CheckFormatSupport(D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
        CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) &&
        CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
}

void D3D12Texture::CheckFeatureSupport() {
    auto device = D3D12GfxDriver::Instance()->GetDevice();

    format_support_.Format = desc_.Format;
    GfxThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,
        &format_support_, sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));
}

uint8_t D3D12Texture::CalculatePlantCount() const {
    auto device = D3D12GfxDriver::Instance()->GetDevice();
    return D3D12GetFormatPlaneCount(device, desc_.Format);
}

UINT D3D12Texture::CalculateNumSubresources() const {
    if (desc_.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) {
        return desc_.MipLevels * desc_.DepthOrArraySize * plant_count_;
    }
    // Buffer only contains 1 subresource
    return 1;
}

void D3D12Texture::CreateTextureFromImage(CommandBuffer* cmd_buffer, const Image& image, bool gen_mips) {
    auto format = image.format();
    uint32_t mip_level = gen_mips ? CalcNumberOfMipLevels(image.width(), image.height()) : 1;

    auto desc = CD3DX12_RESOURCE_DESC::Tex2D(
        image.IsSRGB() ? GetSRGBFormat(format) : format,
        static_cast<UINT64>(image.width()),
        static_cast<UINT>(image.height()),
        static_cast<UINT16>(1),
        mip_level
    );

    auto& scratchImage = image.GetScratchImage();

    auto driver = D3D12GfxDriver::Instance();

    auto allocator = D3D12GfxDriver::Instance()->GetTextureResourceAllocator();
    location_ = allocator->CreateResource(desc, D3D12_RESOURCE_STATE_COMMON, nullptr);

    Initialize(location_);

    std::vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage.GetImageCount());
    const DirectX::Image* pImages = scratchImage.GetImages();
    for (int i = 0; i < scratchImage.GetImageCount(); ++i) {
        auto& subresource = subresources[i];
        subresource.RowPitch = pImages[i].rowPitch;
        subresource.SlicePitch = pImages[i].slicePitch;
        subresource.pData = pImages[i].pixels;
    }

    UploadTexture(cmd_buffer, subresources);

    if (gen_mips && subresources.size() < desc_.MipLevels) {
        cmd_buffer->GenerateMipMaps(this);
    }
}

void D3D12Texture::CreateCubeMapFromImage(CommandBuffer* cmd_buffer, const Image& image, bool gen_mips) {
    cube_map_ = true;

    auto driver = D3D12GfxDriver::Instance();
    auto device = driver->GetDevice();
    auto cmd_list = static_cast<D3D12CommandBuffer*>(cmd_buffer);

    auto cube_width = image.width() / 4;
    auto cube_height = image.height() / 3;
    uint32_t mip_levels = gen_mips ? CalcNumberOfMipLevels(cube_width, cube_height) : 1;

    if (cube_width != cube_height)
    {
        ///TODO: throw exception
        return;
    }

    auto temp_texture = std::make_shared<D3D12Texture>(cmd_buffer, image, gen_mips);
    temp_texture->SetName("Temp Texture for Cube map");

    auto descriptor_allocator = driver->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto temp_srv = descriptor_allocator->Allocate();
    
    device->CreateShaderResourceView(temp_texture->resource_.Get(), nullptr, temp_srv.GetDescriptorHandle());
    cmd_list->TransitionBarrier(temp_texture.get(), ResourceAccessBit::kCopySource);
    
    D3D12_RESOURCE_DESC cubemap_desc = {};
    cubemap_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    cubemap_desc.Width = cube_width;
    cubemap_desc.Height = cube_height;
    cubemap_desc.DepthOrArraySize = 6;
    cubemap_desc.MipLevels = gen_mips ? 0 : 1;
    cubemap_desc.Format = image.IsSRGB() ? GetSRGBFormat(image.format()) : image.format();
    cubemap_desc.SampleDesc.Count = 1;
    cubemap_desc.SampleDesc.Quality = 0;
    cubemap_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    cubemap_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    auto allocator = D3D12GfxDriver::Instance()->GetTextureResourceAllocator();
    location_ = allocator->CreateResource(cubemap_desc, D3D12_RESOURCE_STATE_COMMON, nullptr);

    Initialize(location_);

    cmd_list->TransitionBarrier(this, ResourceAccessBit::kCopyDest);

    D3D12_BOX box;
    box.front = 0;
    box.back = 1;

    D3D12_TEXTURE_COPY_LOCATION copy_src_location;
    copy_src_location.pResource = temp_texture->resource_.Get();
    copy_src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

    D3D12_TEXTURE_COPY_LOCATION copy_dst_location;
    copy_dst_location.pResource = location_.GetResource().Get();
    copy_dst_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

    for (uint32_t i = 0; i < mip_levels; ++i) {
        // +X
        box.left = cube_width * 2;
        box.top = cube_width;
        box.right = cube_width * 3;
        box.bottom = cube_width * 2;

        copy_src_location.SubresourceIndex = i;
        copy_dst_location.SubresourceIndex = CalcSubresource(i, 0, mip_levels);
        cmd_list->CopyTextureRegion(&copy_dst_location, 0, 0, 0, &copy_src_location, &box);

        // -X
        box.left = 0;
        box.top = cube_width;
        box.right = cube_width;
        box.bottom = cube_width * 2;

        copy_dst_location.SubresourceIndex = CalcSubresource(i, 1, mip_levels);
        cmd_list->CopyTextureRegion(&copy_dst_location, 0, 0, 0, &copy_src_location, &box);

        // +Y
        box.left = cube_width;
        box.top = 0;
        box.right = cube_width * 2;
        box.bottom = cube_width;

        copy_dst_location.SubresourceIndex = CalcSubresource(i, 2, mip_levels);
        cmd_list->CopyTextureRegion(&copy_dst_location, 0, 0, 0, &copy_src_location, &box);

        // -Y
        box.left = cube_width;
        box.top = cube_width * 2;
        box.right = cube_width * 2;
        box.bottom = cube_width * 3;

        copy_dst_location.SubresourceIndex = CalcSubresource(i, 3, mip_levels);
        cmd_list->CopyTextureRegion(&copy_dst_location, 0, 0, 0, &copy_src_location, &box);

        // +Z
        box.left = cube_width;
        box.top = cube_width;
        box.right = cube_width * 2;
        box.bottom = cube_width * 2;

        copy_dst_location.SubresourceIndex = CalcSubresource(i, 4, mip_levels);
        cmd_list->CopyTextureRegion(&copy_dst_location, 0, 0, 0, &copy_src_location, &box);

        // -Z
        box.left = cube_width * 3;
        box.top = cube_width;
        box.right = cube_width * 4;
        box.bottom = cube_width * 2;

        copy_dst_location.SubresourceIndex = CalcSubresource(i, 5, mip_levels);
        cmd_list->CopyTextureRegion(&copy_dst_location, 0, 0, 0, &copy_src_location, &box);

        // next mip level texture size
        cube_width /= 2;
    }

    cmd_list->AddInflightResource(temp_texture);
}

void D3D12Texture::UploadTexture(CommandBuffer* cmd_buffer, const std::vector<D3D12_SUBRESOURCE_DATA>& subresources) {

    //GetCopyableFootprints
    const UINT num_subresources = (UINT)subresources.size();
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(num_subresources);
    std::vector<uint32_t> num_rows(num_subresources);
    std::vector<uint64_t> raw_size_in_bytes(num_subresources);

    uint64_t required_size = 0;
    auto driver = D3D12GfxDriver::Instance();
    auto device = driver->GetDevice();
    device->GetCopyableFootprints(&desc_, 0, num_subresources, 0, &layouts[0], &num_rows[0], &raw_size_in_bytes[0], &required_size);

    //Create upload resource
    auto upload_allocator = driver->GetUploadBufferAllocator();
    ResourceLocation upload_buffer_location = upload_allocator->CreateResource((uint32_t)required_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    ID3D12Resource* upload_buffer = upload_buffer_location.GetResource().Get();
    void* mapped_address = upload_buffer_location.GetMappedAddress();

    //Copy contents to upload resource
    for (uint32_t i = 0; i < num_subresources; ++i) {
        if (raw_size_in_bytes[i] > SIZE_T(-1)) {
            assert(0);
        }
        D3D12_MEMCPY_DEST dest_data = {
            (BYTE*)mapped_address + layouts[i].Offset,
            layouts[i].Footprint.RowPitch,
            SIZE_T(layouts[i].Footprint.RowPitch) * SIZE_T(num_rows[i])
        };
        MemcpySubresource(&dest_data, &(subresources[i]), 
            static_cast<SIZE_T>(raw_size_in_bytes[i]), num_rows[i], layouts[i].Footprint.Depth);
    }

    auto cmd_list = static_cast<D3D12CommandBuffer*>(cmd_buffer);
    //Copy data from upload resource to default resource
    cmd_list->TransitionBarrier(this, ResourceAccessBit::kCopyDest);

    for (UINT i = 0; i < num_subresources; ++i) {
        //layouts[i].Offset += upload_buffer_location.block.align_offset;

        CD3DX12_TEXTURE_COPY_LOCATION src;
        src.pResource = upload_buffer;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = layouts[i];

        CD3DX12_TEXTURE_COPY_LOCATION dest;
        dest.pResource = resource_.Get();
        dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dest.SubresourceIndex = i;

        cmd_list->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);
    }

    cmd_list->AddInflightResource(std::move(upload_buffer_location));
}

void D3D12Texture::Create(const TextureDescription& detail, const D3D12_CLEAR_VALUE* clear_value) {
    D3D12_RESOURCE_FLAGS create_flags = GetCreateFlags(detail.create_flags);

    //Create default resource
    auto dxgi_format = GetUnderlyingFormat(detail.format);

    D3D12_RESOURCE_DESC desc;
    ZeroMemory(&desc, sizeof(D3D12_RESOURCE_DESC));
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = detail.width;
    desc.Height = detail.height;
    desc.DepthOrArraySize = detail.depth_or_array_size;
    desc.MipLevels = detail.gen_mips ? 0 : 1;
    desc.Format = dxgi_format;
    desc.SampleDesc.Count = detail.sample_count;
    desc.SampleDesc.Quality = detail.sample_quality;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = create_flags;

    if (detail.type == TextureType::kTexture1D) {
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
    }
    else if (detail.type == TextureType::kTexture3D) {
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    }

    Create(desc, clear_value);
}

void D3D12Texture::Create(const D3D12_RESOURCE_DESC& desc, const D3D12_CLEAR_VALUE* clear_value) {
    auto create_flags = desc.Flags;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;

    if ((create_flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) > 0 ||
        (create_flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) > 0 ||
        (create_flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) > 0) {

        auto prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        GfxThrowIfFailed(D3D12GfxDriver::Instance()->GetDevice()->CreateCommittedResource(
            &prop,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            state,
            clear_value,
            IID_PPV_ARGS(&resource_)));
        Initialize(resource_, state);
    }
    else {
        auto TextureResourceAllocator = D3D12GfxDriver::Instance()->GetTextureResourceAllocator();
        location_ = TextureResourceAllocator->CreateResource(desc, state, clear_value);

        Initialize(location_);
    }

    desc_ = resource_->GetDesc();
}

void D3D12Texture::CreateViews() {
    auto driver = D3D12GfxDriver::Instance();
    auto device = driver->GetDevice();

    if (((desc_.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0 && 
        CheckFormatSupport(D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE)) ||
        force_srv_)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Format = GetSRVFormat(desc_.Format);
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        if (cube_map_) {
            assert(desc_.DepthOrArraySize == 6);

            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srv_desc.TextureCube.MostDetailedMip = 0;
            srv_desc.TextureCube.MipLevels = (uint32_t)desc_.MipLevels;
            srv_desc.TextureCube.ResourceMinLODClamp = 0.0f;
        } else {
            srv_desc.ViewDimension = desc_.SampleDesc.Count > 1 ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MostDetailedMip = 0;
            srv_desc.Texture2D.MipLevels = -1;
        }

        auto descriptor_allocator = driver->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        descriptor_slot_ = descriptor_allocator->Allocate();
        device->CreateShaderResourceView(resource_.Get(), &srv_desc, descriptor_slot_.GetDescriptorHandle());
    }

    // Create UAV for each mip (only supported for 1D and 2D textures).
    if ((desc_.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0 && desc_.DepthOrArraySize == 1 &&
        CheckFormatSupport(D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
        CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) &&
        CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))
    {
        auto descriptor_allocator = driver->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        descriptor_slot_ = descriptor_allocator->Allocate(desc_.MipLevels);
        for (int i = 0; i < desc_.MipLevels; ++i) {
            auto uav_desc = GetUavDesc(desc_, i);
            device->CreateUnorderedAccessView(resource_.Get(), nullptr, &uav_desc, descriptor_slot_.GetDescriptorHandle(i));
        }
    }
}

bool D3D12Texture::Resize(uint32_t width, uint32_t height) {
    assert(width > 0 && height > 0);

    if (desc_.Width == width && desc_.Height == height) {
        return false;
    }

    desc_.Width = width;
    desc_.Height = height;

    Create(desc_, clear_value_set_ ? &clear_value_ : nullptr);
    CreateViews();

    return true;
}

void D3D12Texture::ReleaseNativeResource() {
    resource_.Reset();
    location_ = {};
    descriptor_slot_ = {};
}

void D3D12Texture::ReadBackImage(CommandBuffer* cmd_buffer, int left, int top,
    int width, int height, int destX, int destY, ReadbackDelegate&& callback)
{
    auto gfx = D3D12GfxDriver::Instance();
    auto device = gfx->GetDevice();
    auto commandList = static_cast<D3D12CommandBuffer*>(cmd_buffer);

    CD3DX12_HEAP_PROPERTIES readBackHeapProperties(D3D12_HEAP_TYPE_READBACK);
    constexpr size_t memAlloc = sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) + sizeof(UINT) + sizeof(UINT64);
    uint8_t layoutBuff[memAlloc];
    auto pLayout = reinterpret_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(layoutBuff);
    auto pRowSizesInBytes = reinterpret_cast<UINT64*>(pLayout + 1);// numberOfResources);
    auto pNumRows = reinterpret_cast<UINT*>(pRowSizesInBytes + 1);// numberOfResources);

    UINT64 totalResourceSize = 0;
    device->GetCopyableFootprints(&desc_, 0, 1, 0,
        pLayout, pNumRows, pRowSizesInBytes, &totalResourceSize);

    auto readback_allocator = gfx->GetReadbackBufferAllocator();
    auto stage_location = readback_allocator->CreateResource(totalResourceSize, desc_.Alignment);

    // Transition the resource if necessary
    commandList->TransitionBarrier(this, ResourceAccessBit::kCopySource);

    // Get the copy target location
    auto stage_res = stage_location.GetResource();
    CD3DX12_TEXTURE_COPY_LOCATION copyDest(stage_res.Get(), *pLayout);
    CD3DX12_TEXTURE_COPY_LOCATION copySrc(resource_.Get(), 0);
    D3D12_BOX src_box;
    src_box.left = left;
    src_box.right = left + width;
    src_box.top = top;
    src_box.bottom = top + height;
    src_box.front = 0;
    src_box.back = 1;
    commandList->CopyTextureRegion(&copyDest, 0, 0, 0, &copySrc, &src_box);

    size_t rowPitch = *pRowSizesInBytes;
    ReadbackTask task{ rowPitch, std::move(stage_location), std::move(callback)};
    gfx->EnqueueReadback(std::move(task));
}

void D3D12Texture::ReadbackTask::Process() {
    uint8_t* data;
    auto staging = stage_location.GetResource();
    GfxThrowIfFailed(staging->Map(0, nullptr, reinterpret_cast<void**>(&data)));

    callback(data, raw_pitch);

    staging->Unmap(0, nullptr);
}

}
}
