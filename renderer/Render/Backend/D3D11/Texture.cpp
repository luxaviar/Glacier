#include "texture.h"
#include "render/image.h"
#include "common/color.h"
#include "render/backend/d3d11/gfxdriver.h"
#include "util.h"

namespace glacier {
namespace render {

D3D11Texture::D3D11Texture(const TextureDescription& desc) : Texture(desc) {
    if (!desc.file.empty()) {
        CreateFromFile();
    } else if (desc.use_color) {
        CreateFromColor();
    } else {
        Create();
    }
}

D3D11Texture::D3D11Texture(SwapChain* swapchain) : Texture({}) {
    texture_ = static_cast<D3D11SwapChain*>(swapchain)->GetBackBuffer();
    D3D11_TEXTURE2D_DESC tex_desc;
    texture_->GetDesc(&tex_desc);

    detail_.is_backbuffer = true;
    detail_.type = TextureType::kTexture2D;
    detail_.width = tex_desc.Width;
    detail_.height = tex_desc.Height;

    CreateViews(tex_desc);
}

void D3D11Texture::Reset(const ComPtr<ID3D11Texture2D>& res) {
    texture_ = res;
    D3D11_TEXTURE2D_DESC tex_desc;
    texture_->GetDesc(&tex_desc);

    detail_.width = tex_desc.Width;
    detail_.height = tex_desc.Height;

    CreateViews(tex_desc);
}

uint32_t D3D11Texture::GetMipLevels() const {
    assert(texture_);

    D3D11_TEXTURE2D_DESC desc;
    texture_->GetDesc(&desc);
    return desc.MipLevels;
}

void D3D11Texture::CreateFromColor() {
    detail_.type = TextureType::kTexture2D;

    auto dxgi_format = DXGI_FORMAT_R8G8B8A8_UNORM;
    ColorRGBA32 col = detail_.color;
    Image image(detail_.width, detail_.height, false);
    image.Clear(col);
    
    bool gen_mips = detail_.gen_mips;

    // create texture resource
    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.ArraySize = 1;
    tex_desc.MipLevels = gen_mips ? 0 : 1;
    tex_desc.Format = dxgi_format;
    tex_desc.SampleDesc.Count = detail_.sample_count;
    tex_desc.SampleDesc.Quality = detail_.sample_quality;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | GetCreateFlags(detail_.create_flags);
    tex_desc.CPUAccessFlags = 0;
    tex_desc.MiscFlags = gen_mips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;

    auto dev = D3D11GfxDriver::Instance()->GetDevice();
    
    tex_desc.Width = detail_.width;
    tex_desc.Height = detail_.height;

    D3D11_SUBRESOURCE_DATA subres_data;
    subres_data.pSysMem = image.GetRawPtr<uint8_t>();
    subres_data.SysMemPitch = image.row_pitch_bytes();
    subres_data.SysMemSlicePitch = 0;

    GfxThrowIfFailed(dev->CreateTexture2D(&tex_desc, gen_mips ? nullptr : &subres_data, &texture_));

    CreateViews(tex_desc);

    // generate the mip chain using the gpu rendering pipeline
    if (gen_mips) {
        auto context = D3D11GfxDriver::Instance()->GetContext();
        // write image data into top mip level
        context->UpdateSubresource(texture_.Get(), 0u, nullptr,
            image.GetRawPtr<uint8_t>(), image.row_pitch_bytes(), image.slice_pitch_bytes());
        context->GenerateMips(srv_.Get());
    }
}

void D3D11Texture::Create() {
    auto dxgi_format = GetUnderlyingFormat(detail_.format);
    bool gen_mips = detail_.gen_mips;

    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.ArraySize = detail_.depth_or_array_size; // detail_.type == TextureType::kTexture2D ? 1 : 6;
    tex_desc.MipLevels = gen_mips ? 0 : 1;
    tex_desc.Width = detail_.width;
    tex_desc.Height = detail_.height;
    tex_desc.Format = dxgi_format;
    tex_desc.SampleDesc.Count = detail_.sample_count;
    tex_desc.SampleDesc.Quality = detail_.sample_quality;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = GetCreateFlags(detail_.create_flags);
    tex_desc.CPUAccessFlags = 0;
    tex_desc.MiscFlags = gen_mips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;

    auto dev = D3D11GfxDriver::Instance()->GetDevice();

    UINT support;
    GfxThrowIfFailed(dev->CheckFormatSupport(dxgi_format, &support));
    // special case for typeless depth/stencil buffer
    if (support & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) {
        tex_desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }

    if (support & D3D11_FORMAT_SUPPORT_RENDER_TARGET) {
        tex_desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    }

    if (support & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL) {
        tex_desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
    }

    if (detail_.type == TextureType::kTextureCube) {
        tex_desc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
    }

    GfxThrowIfFailed(dev->CreateTexture2D(&tex_desc, nullptr, &texture_));

    CreateViews(tex_desc);
}

void D3D11Texture::CreateViews(const D3D11_TEXTURE2D_DESC& desc) {
    auto dev = D3D11GfxDriver::Instance()->GetDevice();
    auto srv_fmt = GetSRVFormat(desc.Format);

    // create the resource view on the texture
    if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
        srv_desc.Format = srv_fmt;
        if (detail_.type == TextureType::kTexture2D) {
            srv_desc.ViewDimension = desc.SampleDesc.Count > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
        }
        else {
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        }
        srv_desc.Texture2D.MostDetailedMip = 0;
        srv_desc.Texture2D.MipLevels = -1;
        GfxThrowIfFailed(dev->CreateShaderResourceView(texture_.Get(), &srv_desc, &srv_));
    }

    if (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
        assert(desc.SampleDesc.Count == 1);

        D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
        uav_desc.Format = desc.Format;
        uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        uav_desc.Texture2D.MipSlice = 0;

        GfxThrowIfFailed(dev->CreateUnorderedAccessView(texture_.Get(), &uav_desc, &uav_));
    }
}

void D3D11Texture::CreateFromFile() {
    const auto image = Image(detail_.file.c_str(), detail_.srgb);
    if (detail_.type == TextureType::kTexture2D) {
        CreateTextureFromImage(image, texture_.GetAddressOf(), srv_.GetAddressOf());
    } else {
        CreateCubeMapFromImage(image);
    }
}

void D3D11Texture::CreateTextureFromImage(const Image& image, ID3D11Texture2D** tex, ID3D11ShaderResourceView** srv)
{
    auto format = image.format();
    bool gen_mips = detail_.gen_mips;

    // create texture resource
    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = image.width();
    tex_desc.Height = image.height();
    tex_desc.ArraySize = 1;
    tex_desc.MipLevels = gen_mips ? 0 : 1;
    ///TODO: what about unsupported R32G32B32_FLOAT filter?
    tex_desc.Format = image.IsSRGB() ? GetSRGBFormat(format) : format;
    tex_desc.SampleDesc.Count = detail_.sample_count;
    tex_desc.SampleDesc.Quality = detail_.sample_quality;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | GetCreateFlags(detail_.create_flags);
    tex_desc.CPUAccessFlags = 0;
    tex_desc.MiscFlags = gen_mips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;

    auto device = D3D11GfxDriver::Instance()->GetDevice();
    UINT support;
    GfxThrowIfFailed(device->CheckFormatSupport(tex_desc.Format, &support));
    if (support & D3D11_FORMAT_SUPPORT_RENDER_TARGET) {
        tex_desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    }

    D3D11_SUBRESOURCE_DATA init_data = { 
        image.GetRawPtr<uint8_t>(),
        image.row_pitch_bytes(),
        image.slice_pitch_bytes() 
    };

    GfxThrowIfFailed(device->CreateTexture2D(&tex_desc, gen_mips ? nullptr : &init_data, tex));

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = tex_desc.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = gen_mips ? -1 : 1;
    GfxThrowIfFailed(device->CreateShaderResourceView(*tex, &srv_desc, srv));

    if (gen_mips) {
        auto context = D3D11GfxDriver::Instance()->GetContext();

        context->UpdateSubresource(*tex, 0u, nullptr,
            image.GetRawPtr<uint8_t>(), image.row_pitch_bytes(), image.slice_pitch_bytes());
        context->GenerateMips(*srv);
    }
}

void D3D11Texture::CreateCubeMapFromImage(const Image& image) {
    auto dev = D3D11GfxDriver::Instance()->GetDevice();
    auto context = D3D11GfxDriver::Instance()->GetContext();
    bool gen_mips = detail_.gen_mips;

    ComPtr<ID3D11Texture2D> src_tex;
    ComPtr<ID3D11ShaderResourceView> src_srv;

    CreateTextureFromImage(image, src_tex.GetAddressOf(), src_srv.GetAddressOf());

    D3D11_TEXTURE2D_DESC tex_desc, tex_array_desc;
    src_tex->GetDesc(&tex_desc);

    if (tex_desc.Width * 3 != tex_desc.Height * 4)
    {
        ///TODO: throw exception
        return;
    }

    UINT cube_size = tex_desc.Width / 4;
    tex_array_desc.Width = cube_size;
    tex_array_desc.Height = cube_size;
    tex_array_desc.MipLevels = (gen_mips ? tex_desc.MipLevels - 2 : 1); // 立方体的mip等级比整张位图的少2
    tex_array_desc.ArraySize = 6;
    tex_array_desc.Format = tex_desc.Format;
    tex_array_desc.SampleDesc.Count = 1;
    tex_array_desc.SampleDesc.Quality = 0;
    tex_array_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_array_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | GetCreateFlags(detail_.create_flags);
    tex_array_desc.CPUAccessFlags = 0;
    tex_array_desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    GfxThrowIfFailed(dev->CreateTexture2D(&tex_array_desc, nullptr, &texture_));
    
    D3D11_BOX box;
    box.front = 0;
    box.back = 1;

    for (UINT i = 0; i < tex_array_desc.MipLevels; ++i) {
        // +X
        box.left = cube_size * 2;
        box.top = cube_size;
        box.right = cube_size * 3;
        box.bottom = cube_size * 2;
        context->CopySubresourceRegion(texture_.Get(),
            D3D11CalcSubresource(i, D3D11_TEXTURECUBE_FACE_POSITIVE_X, tex_array_desc.MipLevels),
            0, 0, 0, src_tex.Get(), i, &box);

        // -X
        box.left = 0;
        box.top = cube_size;
        box.right = cube_size;
        box.bottom = cube_size * 2;
        context->CopySubresourceRegion(
            texture_.Get(),
            D3D11CalcSubresource(i, D3D11_TEXTURECUBE_FACE_NEGATIVE_X, tex_array_desc.MipLevels),
            0, 0, 0, src_tex.Get(), i, &box);

        // +Y
        box.left = cube_size;
        box.top = 0;
        box.right = cube_size * 2;
        box.bottom = cube_size;
        context->CopySubresourceRegion(
            texture_.Get(),
            D3D11CalcSubresource(i, D3D11_TEXTURECUBE_FACE_POSITIVE_Y, tex_array_desc.MipLevels),
            0, 0, 0, src_tex.Get(), i, &box);

        // -Y
        box.left = cube_size;
        box.top = cube_size * 2;
        box.right = cube_size * 2;
        box.bottom = cube_size * 3;
        context->CopySubresourceRegion(
            texture_.Get(),
            D3D11CalcSubresource(i, D3D11_TEXTURECUBE_FACE_NEGATIVE_Y, tex_array_desc.MipLevels),
            0, 0, 0, src_tex.Get(), i, &box);

        // +Z
        box.left = cube_size;
        box.top = cube_size;
        box.right = cube_size * 2;
        box.bottom = cube_size * 2;
        context->CopySubresourceRegion(
            texture_.Get(),
            D3D11CalcSubresource(i, D3D11_TEXTURECUBE_FACE_POSITIVE_Z, tex_array_desc.MipLevels),
            0, 0, 0, src_tex.Get(), i, &box);

        // -Z
        box.left = cube_size * 3;
        box.top = cube_size;
        box.right = cube_size * 4;
        box.bottom = cube_size * 2;
        context->CopySubresourceRegion(
            texture_.Get(),
            D3D11CalcSubresource(i, D3D11_TEXTURECUBE_FACE_NEGATIVE_Z, tex_array_desc.MipLevels),
            0, 0, 0, src_tex.Get(), i, &box);

        // next miplevel texture size
        cube_size /= 2;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
    srv_desc.Format = tex_array_desc.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srv_desc.TextureCube.MostDetailedMip = 0;
    srv_desc.TextureCube.MipLevels = tex_array_desc.MipLevels;

    GfxThrowIfFailed(dev->CreateShaderResourceView(texture_.Get(), &srv_desc, &srv_));
}

void D3D11Texture::Bind(ShaderType shader_type, uint16_t slot) {
    auto ctx = D3D11GfxDriver::Instance()->GetContext();
    //if (param_type == ShaderParameterType::kTexture && srv_) {
        assert(srv_);
        switch (shader_type)
        {
        case ShaderType::kVertex:
            GfxThrowIfAny(ctx->VSSetShaderResources(slot, 1, srv_.GetAddressOf()));
            break;
        case ShaderType::kHull:
            GfxThrowIfAny(ctx->HSSetShaderResources(slot, 1, srv_.GetAddressOf()));
            break;
        case ShaderType::kDomain:
            GfxThrowIfAny(ctx->DSSetShaderResources(slot, 1, srv_.GetAddressOf()));
            break;
        case ShaderType::kGeometry:
            GfxThrowIfAny(ctx->GSSetShaderResources(slot, 1, srv_.GetAddressOf()));
            break;
        case ShaderType::kPixel:
            GfxThrowIfAny(ctx->PSSetShaderResources(slot, 1, srv_.GetAddressOf()));
            break;
        case ShaderType::kCompute:
            GfxThrowIfAny(ctx->CSSetShaderResources(slot, 1, srv_.GetAddressOf()));
            break;
        }
    //}
    //else if (param_type == ShaderParameterType::kRWTexture && uav_)
    //{
    //    switch (shader_type)
    //    {
    //    case ShaderType::kCompute:
    //        GfxThrowIfAny(ctx->CSSetUnorderedAccessViews(slot, 1, uav_.GetAddressOf(), nullptr));
    //        break;
    //    }
    //}
}

void D3D11Texture::UnBind(ShaderType shader_type, uint16_t slot) {
    ID3D11UnorderedAccessView* uav = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;

    auto ctx = D3D11GfxDriver::Instance()->GetContext();
    //if (param_type == ShaderParameterType::kTexture) {
        switch (shader_type)
        {
        case ShaderType::kVertex:
            GfxThrowIfAny(ctx->VSSetShaderResources(slot, 1, &srv));
            break;
        case ShaderType::kHull:
            GfxThrowIfAny(ctx->HSSetShaderResources(slot, 1, &srv));
            break;
        case ShaderType::kDomain:
            GfxThrowIfAny(ctx->DSSetShaderResources(slot, 1, &srv));
            break;
        case ShaderType::kGeometry:
            GfxThrowIfAny(ctx->GSSetShaderResources(slot, 1, &srv));
            break;
        case ShaderType::kPixel:
            GfxThrowIfAny(ctx->PSSetShaderResources(slot, 1, &srv));
            break;
        case ShaderType::kCompute:
            GfxThrowIfAny(ctx->CSSetShaderResources(slot, 1, &srv));
            break;
        }
    //}
    //else if (param_type == ShaderParameterType::kRWTexture)
    //{
    //    switch (shader_type)
    //    {
    //    case ShaderType::kCompute:
    //        GfxThrowIfAny(ctx->CSSetUnorderedAccessViews(slot, 1, &uav, nullptr));
    //        break;
    //    }
    //}
}

void D3D11Texture::GenerateMipMaps() {
    D3D11GfxDriver::Instance()->GetContext()->GenerateMips(srv_.Get());
}

void D3D11Texture::ReleaseUnderlyingResource() {
    if (texture_) texture_.Reset();
    if (srv_) srv_.Reset();
    if (uav_) uav_.Reset();
}

bool D3D11Texture::Resize(uint32_t width, uint32_t height) {
    assert(width > 0 && height > 0);
    assert(!detail_.is_backbuffer);
    assert(detail_.file.empty());

    if (detail_.is_backbuffer || !detail_.file.empty()) {
        return false;
    }

    detail_.width = width;
    detail_.height = height;

    if (detail_.use_color) {
        CreateFromColor();
    } else {
        Create();
    }

    return true;
}

void D3D11Texture::ReadBackImage(int left, int top,
    int width, int height, int destX, int destY, ReadbackDelegate&& callback)
{
    D3D11_TEXTURE2D_DESC res_desc;
    texture_->GetDesc(&res_desc);

    auto dev = D3D11GfxDriver::Instance()->GetDevice();
    auto ctx = D3D11GfxDriver::Instance()->GetContext();

    D3D11_TEXTURE2D_DESC staging_desc;
    staging_desc.Width = width;
    staging_desc.Height = height;
    staging_desc.MipLevels = 1;
    staging_desc.ArraySize = 1;
    staging_desc.Format = GetSRVFormat(res_desc.Format);
    staging_desc.SampleDesc.Count = 1;
    staging_desc.SampleDesc.Quality = 0;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.BindFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> staging_tex;
    GfxThrowIfFailed(dev->CreateTexture2D(&staging_desc, NULL, &staging_tex));

    if (res_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
        GfxThrowIfAny(ctx->CopySubresourceRegion(staging_tex.Get(), 0, 0, 0, 0, texture_.Get(), 0, nullptr));
    }
    else {
        D3D11_BOX src_box;
        src_box.left = left;
        src_box.right = left + width;
        src_box.top = top;
        src_box.bottom = top + height;
        src_box.front = 0;
        src_box.back = 1;
        GfxThrowIfAny(ctx->CopySubresourceRegion(staging_tex.Get(), 0, 0, 0, 0, texture_.Get(), 0, &src_box));
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    GfxThrowIfFailed(ctx->Map(staging_tex.Get(), 0, D3D11_MAP_READ, 0, &mapped));

    callback((const uint8_t*)mapped.pData, mapped.RowPitch);

    ctx->Unmap(staging_tex.Get(), 0);
}

//void Texture::CreateCubeMapFromFiles(Graphics& gfx, const std::vector<std::wstring>& paths, D3D11_TEXTURE2D_DESC& tex_desc, bool gen_mips) {
//    assert(paths.size() == 6);
//    auto& dev = gfx.device();
//
//    // load 6 surfaces for cube faces
//    std::vector<Image> images;
//    for (int i = 0; i < 6; i++) {
//        images.push_back(Image(paths[i].c_str()));
//    }
//
//    tex_desc.Width = width_ = images[0].width();
//    tex_desc.Height = height_ = images[0].height();
//
//    D3D11_SUBRESOURCE_DATA data[6];
//    for (int i = 0; i < 6; i++)
//    {
//        data[i].pSysMem = images[i].GetRawPtr<ColorRGBA>();
//        data[i].SysMemPitch = images[i].row_pitch_bytes();
//        data[i].SysMemSlicePitch = 0;
//    }
//
//    GfxThrowIfFailed(dev->CreateTexture2D(&tex_desc, data, &texture_));
//
//    // create the resource view on the texture
//    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
//    srv_desc.Format = tex_desc.Format;
//    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
//    srv_desc.Texture2D.MostDetailedMip = 0;
//    srv_desc.Texture2D.MipLevels = 1;
//    GfxThrowIfFailed(dev->CreateShaderResourceView(texture_.Get(), &srv_desc, &srv_));
//
//    if (gen_mips) {
//        D3D11_TEXTURE2D_DESC desc;
//        texture_->GetDesc(&desc);
//        int mipmap_count = desc.MipLevels;
//
//        for (int i = 0; i < 6; i++) {
//            gfx.context()->UpdateSubresource(texture_.Get(), D3D11CalcSubresource(0, i, mipmap_count), 0, data[0].pSysMem, data[0].SysMemPitch, 0);
//        }
//        gfx.context()->GenerateMips(srv_.Get());
//    }
//}

}
}

