#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <iterator>
#include <thread>
#include <DirectXMath.h>
#include "Math/Mat4.h"
#include "Math/Vec4.h"

namespace glacier {

#ifdef  _UNICODE
using EngineString = std::wstring;
#define EngineToString(x) std::to_wstring(x)
#else 
using EngineString = std::string;
#define EngineToString(x) std::to_string(x)
#endif //  _UNICODE

std::wstring ToWide( const std::string& narrow );

std::string ToNarrow( const std::wstring& wide );

//
// Get the absolute path to the running exe.
//
inline void GetWorkingDir(_Out_writes_z_(pathSize) TCHAR* path, UINT pathSize) {
    if (path == nullptr) {
        return;
    }

    DWORD size = GetModuleFileName(nullptr, path, pathSize);

    if (size == 0 || size == pathSize) {
        // Method failed or path was truncated.
        *path = TEXT('\0');
        return;
    }

#ifdef UNICODE
    TCHAR* lastSlash = wcsrchr(path, L'\\');
#else
    TCHAR* lastSlash = strrchr(path, '\\');
#endif
    if (lastSlash) {
        *(lastSlash + 1) = TEXT('\0');
    }
}


// Set the name of an std::thread.
// Useful for debugging.
const DWORD MS_VC_EXCEPTION = 0x406D1388;

// Set the name of a running thread (for debugging)
#pragma pack( push, 8 )
typedef struct tagTHREADNAME_INFO
{
    DWORD  dwType;      // Must be 0x1000.
    LPCSTR szName;      // Pointer to name (in user addr space).
    DWORD  dwThreadID;  // Thread ID (-1=caller thread).
    DWORD  dwFlags;     // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack( pop )

inline void SetThreadName(std::thread& thread, const char* threadName)
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName;
    info.dwThreadID = ::GetThreadId(reinterpret_cast<HANDLE>(thread.native_handle()));
    info.dwFlags = 0;

    __try
    {
        ::RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

#define _KiB(x) (x * 1024)
#define _MiB(x) (x * 1024 * 1024)
#define _GiB(x) (x * 1024 * 1024 * 1024)

#define _64KB _KiB(64)
#define _1MB _MiB(1)
#define _2MB _MiB(2)
#define _4MB _MiB(4)
#define _8MB _MiB(8)
#define _16MB _MiB(16)
#define _32MB _MiB(32)
#define _64MB _MiB(64)
#define _128MB _MiB(128)
#define _256MB _MiB(256)

}