#include "exception.h"
#include <sstream>
#include "dxerr.h"

namespace glacier {

BaseException::BaseException(int line, const TCHAR* file, HRESULT hr) noexcept :
    line_(line),
    file_(file),
    hr_(hr)
{}

BaseException::BaseException(int line, const TCHAR* file, const char* desc) noexcept :
    line_(line),
    file_(file),
    hr_(S_OK),
    desc_(desc ? desc : "")
{}

BaseException::BaseException(int line, const TCHAR* file, const std::string& desc) noexcept :
    line_(line),
    file_(file),
    hr_(S_OK),
    desc_(desc)
{
}

BaseException::BaseException(int line, const TCHAR* file, HRESULT hr, const char* desc) noexcept :
    line_(line),
    file_(file),
    hr_(hr),
    desc_(desc)
{}

BaseException::BaseException(const std::exception& exp) noexcept :
    BaseException(exp.what())
{
}

BaseException::BaseException(const char* what) noexcept :
    BaseException(0, TEXT(""), "")
{
    what_ = what;
}

// Window Exception Stuff
EngineString BaseException::TranslateErrorCode(HRESULT hr) const noexcept {
    TCHAR* msg_buf = nullptr;
    // windows will allocate memory for err string and make our pointer point to it
    const DWORD msg_len = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPTSTR>(&msg_buf), 0, nullptr
    );
    // 0 string length returned indicates a failure
    if (msg_len == 0) {
        return TEXT("Unidentified error code");
    }
    // copy error string from windows-allocated buffer to std::string
    EngineString errorString = msg_buf;
    // free windows buffer
    LocalFree(msg_buf);
    return errorString;
}

std::string BaseException::GetSource() const noexcept {
    std::ostringstream oss;
    oss << "[File] " << ToNarrow(file_) << std::endl
        << "[Line] " << line_;
    return oss.str();
}

EngineString BaseException::GetDescription() const noexcept {
    return hr_ == S_OK ? ToWide(desc_) : TranslateErrorCode(hr_);
}

const char* BaseException::what() const noexcept {
    std::ostringstream oss;
    oss << type() << std::endl;

    if (hr_ != S_OK) {
        oss << "[Error Code] 0x" << std::hex << std::uppercase << hr_
            << std::dec << " (" << (unsigned long)hr_ << ")" << std::endl;
    }

    oss << "[Description] " << ToNarrow(GetDescription()) << std::endl
    << GetSource();
    what_ = oss.str();
    return what_.c_str();
}


// Graphics exception stuff
GraphicsException::GraphicsException(int line, const TCHAR* file, HRESULT hr,
    std::string&& detail) noexcept :
    BaseException(line, file, hr),
    detail_(std::move(detail))
{
}

GraphicsException::GraphicsException(int line, const TCHAR* file, std::string&& detail) noexcept :
    GraphicsException(line, file, S_OK, std::move(detail))
{
}

const char* GraphicsException::what() const noexcept {
    std::ostringstream oss;
    oss << type() << std::endl;

    if (hr_ != S_OK) {
        oss << "[Error Code] 0x" << std::hex << std::uppercase << hr_
            << std::dec << " (" << (unsigned long)hr_ << ")" << std::endl
            << "[Error String] " << ToNarrow(GetErrorString()) << std::endl
            << "[Description] " << ToNarrow(GetDescription()) << std::endl;
    }

    if (!detail_.empty()) {
        oss << "[Error Detail]" << std::endl
            << GetErrorDetail() << std::endl << std::endl;
    }

    oss << GetSource();
    what_ = oss.str();
    return what_.c_str();
}

EngineString GraphicsException::GetErrorString() const noexcept {
    return DXGetErrorString(hr_);
}

EngineString GraphicsException::GetDescription() const noexcept {
    TCHAR buf[512];
    DXGetErrorDescription(hr_, buf, sizeof(buf));
    return buf;
}

std::string GraphicsException::GetErrorDetail() const noexcept {
    return detail_;
}

GraphicsDeviceRemovedException::GraphicsDeviceRemovedException(int line, const TCHAR* file, HRESULT hr) :
    GraphicsException(line, file, hr)
{
}

}
