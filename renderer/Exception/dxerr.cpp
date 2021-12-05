#include "dxerr.h"

#include <stdio.h>
#include <algorithm>

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
#include <ddraw.h>
#include <d3d9.h>

#define DIRECTINPUT_VERSION 0x800
#include <dinput.h>
#include <dinputd.h>
#endif

#if !defined(WINAPI_FAMILY) || WINAPI_FAMILY != WINAPI_FAMILY_PHONE_APP
#include <wincodec.h>
#include <d2derr.h>
#include <dwrite.h>
#endif
#include <strsafe.h>

//-----------------------------------------------------------------------------
#define BUFFER_SIZE 3000
#define XAUDIO2_E_INVALID_CALL          0x88960001
#define XAUDIO2_E_XMA_DECODER_ERROR     0x88960002
#define XAUDIO2_E_XAPO_CREATION_FAILED  0x88960003
#define XAUDIO2_E_DEVICE_INVALIDATED    0x88960004

#define XAPO_E_FORMAT_UNSUPPORTED MAKE_HRESULT(SEVERITY_ERROR, 0x897, 0x01)

#define DXUTERR_NODIRECT3D              MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0901)
#define DXUTERR_NOCOMPATIBLEDEVICES     MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0902)
#define DXUTERR_MEDIANOTFOUND           MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0903)
#define DXUTERR_NONZEROREFCOUNT         MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0904)
#define DXUTERR_CREATINGDEVICE          MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0905)
#define DXUTERR_RESETTINGDEVICE         MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0906)
#define DXUTERR_CREATINGDEVICEOBJECTS   MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0907)
#define DXUTERR_RESETTINGDEVICEOBJECTS  MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0908)
#define DXUTERR_INCORRECTVERSION        MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0909)
#define DXUTERR_DEVICEREMOVED           MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x090A)

//-----------------------------------------------------------------------------
#define BUFFER_SIZE 3000

#pragma warning( disable : 6001 6221 )

//--------------------------------------------------------------------------------------
#define  CHK_ERR_W(hrchk, strOut) \
        case hrchk: \
             return L##strOut;

#define  CHK_ERRA_W(hrchk) \
        case hrchk: \
             return L#hrchk;

#define  CHK_ERR_A(hrchk, strOut) \
        case hrchk: \
             return strOut;

#define  CHK_ERRA_A(hrchk) \
        case hrchk: \
             return #hrchk;

#define HRESULT_FROM_WIN32b(x) ((HRESULT)(x) <= 0 ? ((HRESULT)(x)) : ((HRESULT) (((x) & 0x0000FFFF) | (FACILITY_WIN32 << 16) | 0x80000000)))

#define  CHK_ERR_WIN32A_W(hrchk) \
        case HRESULT_FROM_WIN32b(hrchk): \
        case hrchk: \
             return L#hrchk;

#define  CHK_ERR_WIN32_ONLY_W(hrchk, strOut) \
        case HRESULT_FROM_WIN32b(hrchk): \
             return L##strOut;

#define  CHK_ERR_WIN32A_A(hrchk) \
        case HRESULT_FROM_WIN32b(hrchk): \
        case hrchk: \
             return #hrchk;

#define  CHK_ERR_WIN32_ONLY_A(hrchk, strOut) \
        case HRESULT_FROM_WIN32b(hrchk): \
             return strOut;

//-----------------------------------------------------
const WCHAR* WINAPI DXGetErrorStringW(_In_ HRESULT hr)
{
#define CHK_ERRA CHK_ERRA_W
#define CHK_ERR CHK_ERR_W
#define CHK_ERR_WIN32A CHK_ERR_WIN32A_W
#define CHK_ERR_WIN32_ONLY CHK_ERR_WIN32_ONLY_W
#define DX_STR_WRAP(...) L##__VA_ARGS__
#include "dxgi_error.inl"
#undef DX_STR_WRAP
#undef CHK_ERR_WIN32A
#undef CHK_ERR_WIN32_ONLY
#undef CHK_ERRA
#undef CHK_ERR
}

const CHAR* WINAPI DXGetErrorStringA(_In_ HRESULT hr)
{
#define CHK_ERRA CHK_ERRA_A
#define CHK_ERR CHK_ERR_A
#define CHK_ERR_WIN32A CHK_ERR_WIN32A_A
#define CHK_ERR_WIN32_ONLY CHK_ERR_WIN32_ONLY_A
#define DX_STR_WRAP(s) s
#include "dxgi_error.inl"
#undef DX_STR_WRAP
#undef CHK_ERR_WIN32A
#undef CHK_ERR_WIN32_ONLY
#undef CHK_ERRA
#undef CHK_ERR
}

//--------------------------------------------------------------------------------------
#undef CHK_ERR
#undef CHK_ERRA
#undef HRESULT_FROM_WIN32b
#undef CHK_ERR_WIN32A
#undef CHK_ERR_WIN32_ONLY
#undef CHK_ERRA_W
#undef CHK_ERR_W
#undef CHK_ERRA_A
#undef CHK_ERR_A

#ifdef UNICODE
#define DX_STR_WRAP(...) L##__VA_ARGS__
#define DX_STRNLEN_S wcsnlen_s
#define STR_FMT_SPEC "%ls"
#define STR_RCHR wcsrchr
#else
#define DX_STR_WRAP(s) s
#define DX_STRNLEN_S strnlen_s
#define STR_FMT_SPEC "%s"
#define STR_RCHR strrchr
#endif // UNICODE

void WINAPI DXGetErrorDescription(_In_ HRESULT hr, _Out_cap_(count) TCHAR* desc, _In_ size_t count) {
    if (!count)
        return;

    *desc = 0;
    // First try to see if FormatMessage knows this hr
    UINT icount = static_cast<UINT>(std::min<size_t>(count, 32767));
    DWORD size = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), desc, icount, nullptr);

    TCHAR* errorStr = STR_RCHR(desc, DX_STR_WRAP('\r'));
    if (errorStr) {
        errorStr[0] = DX_STR_WRAP('\0');
    }
}

HRESULT WINAPI DXTrace(_In_z_ const TCHAR* strFile, _In_ DWORD dwLine, _In_ HRESULT hr,
    _In_opt_ const TCHAR* strMsg, _In_ bool bPopMsgBox)
{
    TCHAR strBufferLine[128];
    TCHAR strBufferError[256];
    TCHAR strBuffer[BUFFER_SIZE];

    StringCbPrintf(strBufferLine, 128, DX_STR_WRAP("%lu"), dwLine);
    if (strFile)
    {
        StringCbPrintf(strBuffer, BUFFER_SIZE, DX_STR_WRAP(STR_FMT_SPEC "(" STR_FMT_SPEC "): "), strFile, strBufferLine);
        OutputDebugString(strBuffer);
    }

    size_t nMsgLen = (strMsg) ? DX_STRNLEN_S(strMsg, 1024) : 0;
    if (nMsgLen > 0)
    {
        OutputDebugString(strMsg);
        OutputDebugString(DX_STR_WRAP(" "));
    }

    TCHAR strBufferErrorMsg[256];
    size_t size = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        strBufferErrorMsg, 256, nullptr);
    
    TCHAR* errorStr = STR_RCHR(strBufferError, DX_STR_WRAP('\r'));
    if (errorStr)
    {
        errorStr[0] = DX_STR_WRAP('\0');
    }

    StringCbPrintf(strBufferError, 256, DX_STR_WRAP(STR_FMT_SPEC " (0x%0.8x)"), strBufferErrorMsg, hr);
    StringCbPrintf(strBuffer, BUFFER_SIZE, DX_STR_WRAP("hr=" STR_FMT_SPEC), strBufferError);
    OutputDebugString(strBuffer);

    OutputDebugString(DX_STR_WRAP("\n"));

#if !defined(WINAPI_FAMILY) || (WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP)
    if (bPopMsgBox)
    {
        TCHAR strBufferFile[MAX_PATH];
        StringCchCopy(strBufferFile, MAX_PATH, DX_STR_WRAP(""));
        if (strFile)
            StringCchCopy(strBufferFile, MAX_PATH, strFile);

        TCHAR strBufferMsg[1024];
        StringCchCopy(strBufferMsg, 1024, DX_STR_WRAP(""));
        if (nMsgLen > 0)
            StringCbPrintf(strBufferMsg, 1024, DX_STR_WRAP("Calling: " STR_FMT_SPEC "\n"), strMsg);

        StringCbPrintf(strBuffer, BUFFER_SIZE, DX_STR_WRAP("File: " STR_FMT_SPEC "\nLine: " STR_FMT_SPEC "\nError Code: " STR_FMT_SPEC "\n" STR_FMT_SPEC "Do you want to debug the application?"),
            strBufferFile, strBufferLine, strBufferError, strBufferMsg);

        int nResult = MessageBox(GetForegroundWindow(), strBuffer, DX_STR_WRAP("Unexpected error encountered"), MB_YESNO | MB_ICONERROR);
        if (nResult == IDYES)
            DebugBreak();
    }
#else
    UNREFERENCED_PARAMETER(bPopMsgBox);
#endif

    return hr;

}

#undef DX_STR_WRAP
#undef DX_STRNLEN_S
#undef STR_FMT_SPEC
