#pragma once

#include "Shader.h"
#include <d3dcompiler.h>

namespace glacier {
namespace render {

HRESULT Shader::CompileFromFile(const TCHAR* file_path, ID3DBlob** ptr_blob, const D3D_SHADER_MACRO* defines,
    const char* entry_point, const char* target, UINT flags1, UINT flags2)
{
#if defined(DEBUG) || defined(_DEBUG)
    flags1 |= D3DCOMPILE_DEBUG;
    flags1 |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* error_blob = nullptr;

    HRESULT hr = D3DCompileFromFile(file_path, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry_point, target, flags1, flags2,
        ptr_blob, &error_blob);

    if (error_blob) {
        OutputDebugStringA(reinterpret_cast<const char*>(error_blob->GetBufferPointer()));
        error_blob->Release();
    }

    return hr;
}

}
}
