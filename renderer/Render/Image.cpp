#include "image.h"
#include <algorithm>
#include <cassert>
#include <sstream>
#include <filesystem>
#include "Common/Util.h"
#include "common/color.h"

namespace glacier {
namespace render {

// surface exception stuff
ImageException::ImageException(int line, const TCHAR* file, const TCHAR* filename, HRESULT hr) noexcept :
    BaseException(line, file, hr),
    file_name_(filename)
{

}

ImageException::ImageException(int line, const TCHAR* file, const TCHAR* filename, const char* desc) noexcept :
    BaseException(line, file, desc),
    file_name_(filename)
{
}

const char* ImageException::what() const noexcept {
    std::ostringstream oss;
    oss << BaseException::what() << std::endl 
        << "[Image File] " << ToNarrow(file_name_);
    what_ = oss.str();
    return what_.c_str();
}

const TCHAR* ImageException::type() const noexcept
{
    return TEXT("Image Exception");
}

Image::Image(const TCHAR* file_path, bool is_srgb) : is_srgb_(is_srgb) {
    DirectX::ScratchImage scratch;

    std::wstring_view wsv(file_path);
    std::wstring_view ext;

    auto pos = wsv.rfind(L".");
    if (pos != std::wstring_view::npos) {
        ext = wsv.substr(pos, wsv.size() - pos);
    }

    HRESULT hr;
    if (ext == L".hdr") {
        hr = DirectX::LoadFromHDRFile(file_path, nullptr, scratch);
    }
    else if (ext == L".dds") {
        hr = DirectX::LoadFromDDSFile(file_path, DirectX::DDS_FLAGS_FORCE_RGB, nullptr, scratch);
    }
    else if (ext == L".tga") {
        hr = DirectX::LoadFromTGAFile(file_path, nullptr, scratch);
    }
    else {
        hr = DirectX::LoadFromWICFile(file_path, DirectX::WIC_FLAGS_FORCE_RGB, nullptr, scratch);
    }

    if (FAILED(hr)) {
        throw ImageException(__LINE__, TEXT(__FILE__), file_path, hr);
    }

    //auto img = scratch.GetImage(0, 0, 0);
    //auto predict_fmt = is_hdr ? kHdrFormat : kFormat;

    //if (img->format != predict_fmt)
    //{
    //    DirectX::ScratchImage converted;
    //    hr = DirectX::Convert(
    //        *img,
    //        predict_fmt,
    //        DirectX::TEX_FILTER_DEFAULT,
    //        DirectX::TEX_THRESHOLD_DEFAULT,
    //        converted
    //    );

    //    if (FAILED(hr)) {
    //        throw ImageException(__LINE__, TEXT(__FILE__), file_path, hr);
    //    }

    //    scratch_ = std::move(converted);
    //} else {
        scratch_ = std::move(scratch);
    //}
}

Image::Image(uint32_t width, uint32_t height, bool is_rgb) : is_srgb_(is_rgb) {
    HRESULT hr = scratch_.Initialize2D(kFormat, width, height, 1u, 1u);
    if (FAILED(hr)) {
        throw ImageException( __LINE__,TEXT(__FILE__),TEXT("Failed to initialize ScratchImage"),hr );
    }
}

void Image::Save( const TCHAR* filename, bool ignore_srgb) const {
    DirectX::WICCodecs codec;
    const std::filesystem::path path = filename;
    const auto ext = path.extension().string();

    if( ext == ".png" ) {
        codec = DirectX::WIC_CODEC_PNG;
    } else if( ext == ".jpg" ) {
        codec = DirectX::WIC_CODEC_JPEG;
    } else if( ext == ".bmp" ) {
        codec = DirectX::WIC_CODEC_BMP;
    } else {
        throw ImageException(__LINE__, TEXT(__FILE__), filename, "Image format not supported");
    }

    HRESULT hr = DirectX::SaveToWICFile(
        *scratch_.GetImage( 0,0,0 ),
        ignore_srgb ? DirectX::WIC_FLAGS_IGNORE_SRGB : DirectX::WIC_FLAGS_FORCE_SRGB,
        GetWICCodec(codec),
        filename
    );

    if(FAILED(hr)) {
        throw ImageException( __LINE__, TEXT(__FILE__), filename, hr );
    }
}

}
}
