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
    BaseException(int line, const TCHAR* file, const TCHAR* desc) noexcept;
    BaseException(int line, const TCHAR* file, std::string desc) noexcept;

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
    EngineString desc_;
    mutable std::string what_;
};

class WindowException : public BaseException {
public:
    using BaseException::BaseException;

    const TCHAR* type() const noexcept override {
        return TEXT("Window Exception");
    }
};

class GraphicsException : public BaseException
{
public:
    GraphicsException(int line, const TCHAR* file, HRESULT hr, std::string&& detail = {}) noexcept;
    GraphicsException(int line, const TCHAR* file, std::string&& detail = {}) noexcept;

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

class GraphicsDeviceRemovedException : public GraphicsException
{
public:
    GraphicsDeviceRemovedException(int line, const TCHAR* file, HRESULT hr);

    const TCHAR* type() const noexcept {
        return TEXT("Graphics Exception [Device Removed] (DXGI_ERROR_DEVICE_REMOVED)");
    }
};

}

#define WinThrowLastExcept() throw WindowException(__LINE__,TEXT(__FILE__), GetLastError())

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
