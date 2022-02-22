#pragma once

#include <exception>
#include <string>
#include <vector>
#include "Common/Util.h"
#include "DxgiInfo.h"

namespace glacier {

class BaseException : public std::exception {
public:
    BaseException(int line, const TCHAR* file, HRESULT hr) noexcept;
    BaseException(int line, const TCHAR* file, const char* desc) noexcept;
    BaseException(int line, const TCHAR* file, const std::string& desc) noexcept;
    BaseException(int line, const TCHAR* file, HRESULT hr, const char* desc) noexcept;

    BaseException(const char* what) noexcept;
    BaseException(const std::exception& exp) noexcept;

    BaseException(const BaseException& other) = default;

    virtual ~BaseException() {}

    const char* what() const noexcept override;

    int line() const noexcept { return line_; }
    const EngineString& file() const noexcept { return file_; }
    HRESULT error_code() const noexcept { return hr_; }
    virtual const TCHAR* type() const noexcept { return TEXT("Base Exception"); }

    std::string GetSource() const noexcept;
    virtual EngineString GetDescription() const noexcept;

protected:
    EngineString TranslateErrorCode(HRESULT hr) const noexcept;

    int line_;
    EngineString file_;

    HRESULT hr_;
    std::string desc_;
    mutable std::string what_;
};

class WindowException : public BaseException {
public:
    using BaseException::BaseException;

    const TCHAR* type() const noexcept override {
        return TEXT("Window Exception");
    }
};

class GraphicsException : public BaseException {
public:
    GraphicsException(int line, const TCHAR* file, HRESULT hr, std::string&& detail = {}) noexcept;
    GraphicsException(int line, const TCHAR* file, std::string&& detail = {}) noexcept;

    GraphicsException(const GraphicsException& other) = default;

    const TCHAR* type() const noexcept override {
        return TEXT("Graphics Exception");
    }

    const char* what() const noexcept override;
    EngineString GetDescription() const noexcept override;

    EngineString GetErrorString() const noexcept;
    std::string GetErrorDetail() const noexcept;

private:
    std::string detail_;
};

class GraphicsDeviceRemovedException : public GraphicsException {
public:
    GraphicsDeviceRemovedException(int line, const TCHAR* file, HRESULT hr);
    GraphicsDeviceRemovedException(const GraphicsDeviceRemovedException& other) = default;

    const TCHAR* type() const noexcept {
        return TEXT("Graphics Exception [Device Removed] (DXGI_ERROR_DEVICE_REMOVED)");
    }
};

}

#define WinThrowLastExcept() throw WindowException(__LINE__,TEXT(__FILE__), GetLastError())
#define ThrowIfLastExcept(what) if(HRESULT hr; FAILED(hr = (GetLastError()))) throw BaseException(__LINE__,TEXT(__FILE__), hr, what)
#define ThrowAssert(call, what) if (!(call)) throw BaseException(__LINE__,TEXT(__FILE__), what)
#define ThrowIfFailed(hrcall, what) if(HRESULT hr; FAILED(hr = (hrcall))) throw BaseException(__LINE__, TEXT(__FILE__), hr, what)

#define GfxThrowIfFailedRaw(hrcall) if(HRESULT hr; FAILED(hr = (hrcall))) throw GraphicsException(__LINE__, TEXT(__FILE__), hr)

#ifndef NDEBUG
#define GfxExcept(hr) GraphicsException(__LINE__, TEXT(__FILE__), (hr), DxgiInfo::Instance()->GetMessages())
#define GfxThrowIfFailed(hrcall) DxgiInfo::Instance()->Set(); if(HRESULT hr; FAILED(hr = (hrcall))) throw GfxExcept(hr)
#define GfxThrowCheck(hr) if(FAILED(hr)) throw GfxExcept(hr)
#define GfxThrowIfAny(call) DxgiInfo::Instance()->Set(); (call); {auto v = DxgiInfo::Instance()->GetMessages(); if(!v.empty()) {throw GraphicsException(__LINE__, TEXT(__FILE__), std::move(v));}}
#else
#define GfxExcept(hr) GraphicsException(__LINE__, TEXT(__FILE__), (hr))
#define GfxThrowIfFailed(hrcall) GfxThrowIfFailedRaw(hrcall)
#define GfxThrowCheck(hr) if(FAILED(hr)) throw GfxExcept(hr)
#define GfxThrowIfAny(call) (call)
#endif
