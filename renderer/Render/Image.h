#pragma once

#include <DirectXTex.h>
#include <string>
#include <optional>
#include "exception/exception.h"
#include "Math/Vec4.h"

namespace glacier {
namespace render {

class ImageException : public BaseException
{
public:
    ImageException(int line, const TCHAR* file, const TCHAR* filename, HRESULT hr) noexcept;
    ImageException(int line, const TCHAR* file, const TCHAR* filename, const char* note) noexcept;

    const char* what() const noexcept override;
    const TCHAR* type() const noexcept override;

private:
    EngineString file_name_;
};

class Image {
public:
    static constexpr DXGI_FORMAT kFormat = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
    static constexpr DXGI_FORMAT kHdrFormat = DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT;

    Image(const TCHAR* file_path, bool is_srgb = true);// , bool is_hdr = false);
    Image(uint32_t width, uint32_t height, bool is_rgb =false);// , bool is_hdr = false);
    Image(Image&& source) noexcept = default;
    Image(Image&) = delete;
    ~Image() = default;

    Image& operator=( Image&& donor ) noexcept = default;
    Image& operator=( const Image& ) = delete;

    uint32_t width() const noexcept {
        return (uint32_t)scratch_.GetMetadata().width;
    }

    uint32_t height() const noexcept {
        return (uint32_t)scratch_.GetMetadata().height;
    }

    uint32_t row_pitch_bytes() const noexcept {
        return (uint32_t)scratch_.GetImage(0, 0, 0)->rowPitch;
    }

    uint32_t slice_pitch_bytes() const noexcept {
        return (uint32_t)scratch_.GetImage(0, 0, 0)->slicePitch;
    }

    DXGI_FORMAT format() const {
        return scratch_.GetMetadata().format;
    }

    bool IsOpaque() const noexcept {
        return scratch_.IsAlphaAllOpaque();
    }

    const DirectX::ScratchImage& GetScratchImage() const { return scratch_; }
    const DirectX::TexMetadata& GetMeta() const { return scratch_.GetMetadata(); }

    //bool IsHDR() const { return is_hdr_; }
    bool IsSRGB() const { return is_srgb_; }

    void Save(const TCHAR* filename, bool ignore_srgb = false) const;

    template<typename T>
    void Clear(const T& fill_color) noexcept {
        const auto h = height();
        auto& imgData = *scratch_.GetImage(0, 0, 0);
        for (size_t y = 0; y < h; y++) {
            auto rowStart = reinterpret_cast<T*>(imgData.pixels + imgData.rowPitch * y);
            std::fill(rowStart, rowStart + imgData.width, fill_color);
        }
    }

    template<typename T>
    void SetPixel(uint32_t x, uint32_t y, const T& c) {
        auto ptr = GetRawPtr<T>(x, y);
        *ptr = c;
    }

    template<typename T>
    T GetPixel(uint32_t x, uint32_t y) const {
        auto ptr = GetRawPtr<T>(x, y);
        return *ptr;
    }

    template<typename T>
    T* GetRawPtr(uint32_t x = 0, uint32_t y = 0) {
        assert(x < width());
        assert(y < height());
        auto ptr = scratch_.GetPixels();
        auto row_pitch = scratch_.GetImage(0, 0, 0)->rowPitch;
        ptr += y * row_pitch + x * sizeof(T);
        return reinterpret_cast<T*>(ptr);
    }

    template<typename T>
    const T* GetRawPtr(uint32_t x = 0, uint32_t y = 0) const {
        assert(x < width());
        assert(y < height());
        auto ptr = scratch_.GetPixels();
        auto row_pitch = scratch_.GetImage(0, 0, 0)->rowPitch;
        ptr += y * row_pitch + x * sizeof(T);
        return reinterpret_cast<const T*>(ptr);
    }

private:
    bool is_srgb_ = true;
    //bool is_hdr_ = false;
    DirectX::ScratchImage scratch_;
};

}
}