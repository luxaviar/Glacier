#include "Texture.h"
#include <DirectXHelpers.h>
#include <array>
#include "Exception/Exception.h"
#include "GfxDriver.h"
#include "render/image.h"
#include "math/Vec2.h"
#include "Common/ScopedBuffer.h"

namespace glacier {
namespace render {

D3D12Texture::D3D12Texture(const TextureDescription& desc, const D3D12_CLEAR_VALUE* clear_value) :
    Texture(desc)
{
    if (clear_value) {
        clear_value_ = *clear_value;
        clear_value_set_ = true;
    }

    if (!desc.file.empty()) {
        CreateFromFile();
    }
    else if (desc.use_color) {
        CreateFromColor();
    }
    else {
        Create(clear_value);
    }

    CheckFeatureSupport();
    CreateViews();
}

D3D12Texture::D3D12Texture(ComPtr<ID3D12Resource>& res, D3D12_RESOURCE_STATES state) : Texture({})
{
    resource_ = res;
    state_ = state;

    auto desc = resource_->GetDesc();
    detail_.width = (uint32_t)desc.Width;
    detail_.height = (uint32_t)desc.Height;
    /// FIXME: format setting
    //detail_.format = desc.Format;

    CheckFeatureSupport();
    CreateViews();
}

D3D12Texture::D3D12Texture(SwapChain* swapchain) : Texture({}) {
    resource_ = static_cast<D3D12SwapChain*>(swapchain)->GetBackBuffer();
    auto desc = resource_->GetDesc();

    detail_.type = TextureType::kTexture2D;
    detail_.width = (uint32_t)desc.Width;
    detail_.height = desc.Height;
}

void D3D12Texture::SetName(const TCHAR* name) {
    SetDebugName(name);
}

const TCHAR* D3D12Texture::GetName(const TCHAR* name) const {
    return GetDebugName().c_str();
}

void D3D12Texture::Reset(ComPtr<ID3D12Resource>& res, D3D12_RESOURCE_STATES state) {
    resource_ = res;
    state_ = state;
    auto desc = resource_->GetDesc();

    detail_.width = (uint32_t)desc.Width;
    detail_.height = desc.Height;
}

void D3D12Texture::CreateFromFile() {
    const Image image(detail_.file.c_str(), detail_.srgb);
    if (detail_.type == TextureType::kTexture2D) {
        CreateTextureFromImage(image, location_, D3D12_RESOURCE_STATE_COMMON, detail_.gen_mips);
        resource_ = location_.resource;
        state_ = D3D12_RESOURCE_STATE_COMMON;
    }
    else {
        CreateCubeMapFromImage(image);
    }
    SetDebugName(detail_.file.c_str());
}

void D3D12Texture::CreateTextureFromImage(const Image& image, ResourceLocation& location, D3D12_RESOURCE_STATES state, bool gen_mips) {
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
    if ((detail_.create_flags & (uint32_t)CreateFlags::kUav) > 0) {
        desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    auto driver = D3D12GfxDriver::Instance();

    auto allocator = D3D12GfxDriver::Instance()->GetTextureResourceAllocator();
    location = allocator->AllocResource(desc, state, nullptr);
    assert(location.resource);

    std::vector<D3D12_SUBRESOURCE_DATA> subresources(scratchImage.GetImageCount());
    const DirectX::Image* pImages = scratchImage.GetImages();
    for (int i = 0; i < scratchImage.GetImageCount(); ++i) {
        auto& subresource = subresources[i];
        subresource.RowPitch = pImages[i].rowPitch;
        subresource.SlicePitch = pImages[i].slicePitch;
        subresource.pData = pImages[i].pixels;
    }

    auto cmd_list = driver->GetCommandList();

    UploadTexture(location.resource.Get(), state, subresources);

    if (gen_mips && subresources.size() < location.resource->GetDesc().MipLevels) {
        D3D12Texture temp(location.resource, state);
        driver->GenerateMips(&temp);
    }
}

void D3D12Texture::CreateCubeMapFromImage(const Image& image) {
    auto driver = D3D12GfxDriver::Instance();
    auto device = driver->GetDevice();
    auto cmd_list = driver->GetCommandList();
    bool gen_mips = detail_.gen_mips;

    auto cube_width = image.width() / 4;
    auto cube_height = image.height() / 3;
    uint32_t mip_levels = gen_mips ? CalcNumberOfMipLevels(cube_width, cube_height) : 1;

    if (cube_width != cube_height)
    {
        ///TODO: throw exception
        return;
    }

    ResourceLocation temp_location;
    CreateTextureFromImage(image, temp_location, D3D12_RESOURCE_STATE_COMMON, gen_mips);

    auto descriptor_allocator = driver->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto temp_srv = descriptor_allocator->Allocate();
    device->CreateShaderResourceView(temp_location.resource.Get(), nullptr, temp_srv.GetDescriptorHandle());
    driver->GetCommandQueue()->GetCommandList()->
        TransitionBarrier(temp_location.resource.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
    
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
    location_ = allocator->AllocResource(cubemap_desc, D3D12_RESOURCE_STATE_COMMON, nullptr);

    assert(location_.resource);

    resource_ = location_.resource;
    state_ = D3D12_RESOURCE_STATE_COMMON;
    TransitionBarrier(cmd_list, D3D12_RESOURCE_STATE_COPY_DEST);

    D3D12_BOX box;
    box.front = 0;
    box.back = 1;

    D3D12_TEXTURE_COPY_LOCATION copy_src_location;
    copy_src_location.pResource = temp_location.resource.Get();
    copy_src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

    D3D12_TEXTURE_COPY_LOCATION copy_dst_location;
    copy_dst_location.pResource = location_.resource.Get();
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

    TransitionBarrier(cmd_list, D3D12_RESOURCE_STATE_COMMON);

    driver->AddInflightResource(std::move(temp_location));
}

void D3D12Texture::UploadTexture(ID3D12Resource* tex, D3D12_RESOURCE_STATES state, const std::vector<D3D12_SUBRESOURCE_DATA>& subresources) {

    //GetCopyableFootprints
    const UINT num_subresources = (UINT)subresources.size();
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(num_subresources);
    std::vector<uint32_t> num_rows(num_subresources);
    std::vector<uint64_t> raw_size_in_bytes(num_subresources);

    uint64_t required_size = 0;
    auto desc = tex->GetDesc();

    auto driver = D3D12GfxDriver::Instance();
    auto device = driver->GetDevice();
    device->GetCopyableFootprints(&desc, 0, num_subresources, 0, &layouts[0], &num_rows[0], &raw_size_in_bytes[0], &required_size);

    //Create upload resource
    auto upload_allocator = driver->GetUploadBufferAllocator();
    ResourceLocation upload_buffer_location = upload_allocator->AllocResource((uint32_t)required_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    ID3D12Resource* upload_buffer = upload_buffer_location.GetLocationResource<D3D12UploadBufferAllocator>().GetUnderlyingResource().Get();
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

    auto cmd_queue = driver->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    auto cmd_list = cmd_queue->GetCommandList();
    //Copy data from upload resource to default resource
    cmd_list->TransitionBarrier(tex, state, D3D12_RESOURCE_STATE_COPY_DEST);

    for (UINT i = 0; i < num_subresources; ++i) {
        layouts[i].Offset += upload_buffer_location.block.align_offset;

        CD3DX12_TEXTURE_COPY_LOCATION src;
        src.pResource = upload_buffer;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = layouts[i];

        CD3DX12_TEXTURE_COPY_LOCATION dest;
        dest.pResource = tex;
        dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dest.SubresourceIndex = i;

        cmd_list->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);
    }

    cmd_list->TransitionBarrier(tex, D3D12_RESOURCE_STATE_COPY_DEST, state);
    driver->AddInflightResource(std::move(upload_buffer_location));
}

void D3D12Texture::CreateFromColor() {
    auto dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM;
    ColorRGBA32 col = detail_.color;
    Image image(detail_.width, detail_.height, false);
    image.Clear(col);

    CreateTextureFromImage(image, location_, D3D12_RESOURCE_STATE_COMMON, detail_.gen_mips);
    resource_ = location_.resource;
    state_ = D3D12_RESOURCE_STATE_COMMON;
}

void D3D12Texture::Create(const D3D12_CLEAR_VALUE* clear_value) {
    D3D12_RESOURCE_FLAGS create_flags = GetCreateFlags(detail_.create_flags);

    //Create default resource
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    auto dxgi_format = GetUnderlyingFormat(detail_.format);

    D3D12_RESOURCE_DESC desc;
    ZeroMemory(&desc, sizeof(D3D12_RESOURCE_DESC));
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = detail_.width;
    desc.Height = detail_.height;
    desc.DepthOrArraySize = detail_.depth_or_array_size;
    desc.MipLevels = detail_.gen_mips ? 0 : 1;
    desc.Format = dxgi_format;
    desc.SampleDesc.Count = detail_.sample_count;
    desc.SampleDesc.Quality = detail_.sample_quality;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = create_flags;

    if (detail_.type == TextureType::kTexture1D) {
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
    }
    else if (detail_.type == TextureType::kTexture3D) {
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    }

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
    }
    else {
        auto TextureResourceAllocator = D3D12GfxDriver::Instance()->GetTextureResourceAllocator();
        location_ = TextureResourceAllocator->AllocResource(desc, state, clear_value);

        assert(location_.resource);
        resource_ = location_.resource;
    }


    state_ = state;
}

void D3D12Texture::CreateViews() {
    auto driver = D3D12GfxDriver::Instance();
    auto device = driver->GetDevice();
    auto desc = resource_->GetDesc();

    if (((desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) == 0 && 
        CheckFormatSupport(D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE)) ||
        (detail_.create_flags & (uint32_t)CreateFlags::kShaderResource) > 0)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Format = GetSRVFormat(desc.Format);
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        if (detail_.type == TextureType::kTextureCube) {
            assert(desc.DepthOrArraySize == 6);

            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srv_desc.TextureCube.MostDetailedMip = 0;
            srv_desc.TextureCube.MipLevels = (uint32_t)desc.MipLevels;
            srv_desc.TextureCube.ResourceMinLODClamp = 0.0f;
        } else {// (detail_.type == TextureType::kTexture2D)
            srv_desc.ViewDimension = desc.SampleDesc.Count > 1 ? D3D12_SRV_DIMENSION_TEXTURE2DMS : D3D12_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MostDetailedMip = 0;
            srv_desc.Texture2D.MipLevels = -1;
        }

        auto descriptor_allocator = driver->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        srv_slot_ = descriptor_allocator->Allocate();
        device->CreateShaderResourceView(resource_.Get(), &srv_desc, srv_slot_.GetDescriptorHandle());
    }

    // Create UAV for each mip (only supported for 1D and 2D textures).
    if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0 && desc.DepthOrArraySize == 1 &&
        CheckFormatSupport(D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
        CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) &&
        CheckFormatSupport(D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))
    {
        auto descriptor_allocator = driver->GetDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        uav_slot_ = descriptor_allocator->Allocate(desc.MipLevels);
        for (int i = 0; i < desc.MipLevels; ++i) {
            auto uav_desc = GetUavDesc(desc, i);
            device->CreateUnorderedAccessView(resource_.Get(), nullptr, &uav_desc, uav_slot_.GetDescriptorHandle(i));
        }
    }

    if (detail_.format == TextureFormat::kUnkown) {
        detail_.format = GetTextureFormat(desc.Format);
    }
}

bool D3D12Texture::Resize(uint32_t width, uint32_t height) {
    assert(width > 0 && height > 0);
    assert(!detail_.is_backbuffer);
    assert(detail_.file.empty());

    if (detail_.is_backbuffer || !detail_.file.empty()) {
        return false;
    }

    if (detail_.width == width && detail_.height == height) {
        return false;
    }

    detail_.width = width;
    detail_.height = height;

    if (detail_.use_color) {
        CreateFromColor();
    }
    else {
        Create(clear_value_set_ ? &clear_value_ : nullptr);
    }
    CreateViews();

    return true;
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

uint32_t D3D12Texture::GetMipLevels() const {
    auto desc = resource_->GetDesc();
    return desc.MipLevels;
}

void D3D12Texture::GenerateMipMaps() {
    auto driver = D3D12GfxDriver::Instance();
    driver->GenerateMips(this);
}

void D3D12Texture::ReleaseUnderlyingResource() {
    resource_.Reset();
    location_ = {};
    srv_slot_ = {};
    uav_slot_ = {};
}

void D3D12Texture::ReadBackImage(int left, int top,
    int width, int height, int destX, int destY, ReadbackDelegate&& callback)
{
    auto desc = GetDesc();
    auto gfx = D3D12GfxDriver::Instance();
    auto device = gfx->GetDevice();

    auto commandList = gfx->GetCommandList();

    CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_HEAP_PROPERTIES readBackHeapProperties(D3D12_HEAP_TYPE_READBACK);

    constexpr size_t memAlloc = sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) + sizeof(UINT) + sizeof(UINT64);
    uint8_t layoutBuff[memAlloc];
    auto pLayout = reinterpret_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(layoutBuff);
    auto pRowSizesInBytes = reinterpret_cast<UINT64*>(pLayout + 1);// numberOfResources);
    auto pNumRows = reinterpret_cast<UINT*>(pRowSizesInBytes + 1);// numberOfResources);

    UINT64 totalResourceSize = 0;
    device->GetCopyableFootprints(&desc, 0, 1, 0,
        pLayout, pNumRows, pRowSizesInBytes, &totalResourceSize);

    // Readback resources must be buffers
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Alignment = desc.Alignment;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.Height = 1;
    bufferDesc.Width = totalResourceSize;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.SampleDesc.Quality = 0;

    ComPtr<ID3D12Resource> pStaging;
    ComPtr<ID3D12Resource> copySource(resource_);
    
    // Create a staging texture
    GfxThrowIfFailed(device->CreateCommittedResource(
        &readBackHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_GRAPHICS_PPV_ARGS(pStaging.GetAddressOf())));

    assert(pStaging);

    // Transition the resource if necessary
    commandList->TransitionBarrier(resource_.Get(), state_, D3D12_RESOURCE_STATE_COPY_SOURCE);

    // Get the copy target location
    CD3DX12_TEXTURE_COPY_LOCATION copyDest(pStaging.Get(), *pLayout);
    CD3DX12_TEXTURE_COPY_LOCATION copySrc(copySource.Get(), 0);
    D3D12_BOX src_box;
    src_box.left = left;
    src_box.right = left + width;
    src_box.top = top;
    src_box.bottom = top + height;
    src_box.front = 0;
    src_box.back = 1;
    commandList->CopyTextureRegion(&copyDest, 0, 0, 0, &copySrc, &src_box);

    // Transition the resource to the next state
    commandList->TransitionBarrier(resource_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, state_);

    size_t rowPitch = *pRowSizesInBytes;
    ReadbackTask task{ rowPitch, pStaging, std::move(callback)};
    gfx->EnqueueReadback(std::move(task));
}

void D3D12Texture::ReadbackTask::Process() {
    uint8_t* data;
    GfxThrowIfFailed(staging->Map(0, nullptr, reinterpret_cast<void**>(&data)));

    callback(data, raw_pitch);

    staging->Unmap(0, nullptr);
}

}
}
