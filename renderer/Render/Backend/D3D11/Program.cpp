#include "Program.h"
#include <d3dcompiler.h>
#include "Common/TypeTraits.h"
#include "exception/exception.h"
#include "GfxDriver.h"

namespace glacier {
namespace render {

D3D11Program::D3D11Program(const char* name) : Program(name) {

}

void D3D11Program::SetupShaderParameter(const std::shared_ptr<Shader>& shader) {
    auto sh = std::dynamic_pointer_cast<D3D11Shader>(shader);
    auto blob = sh->GetBytecode();

    // Reflect the parameters from the shader.
    // Inspired by: http://members.gamedev.net/JasonZ/Heiroglyph/D3D11ShaderReflection.pdf
    ComPtr<ID3D11ShaderReflection> reflector;
    GfxThrowIfFailed(D3DReflect(blob->GetBufferPointer(), blob->GetBufferSize(), IID_ID3D11ShaderReflection, &reflector));

    D3D11_SHADER_DESC desc;
    GfxThrowIfFailed(reflector->GetDesc(&desc));

    // Query Resources that are bound to the shader.
    for (UINT i = 0; i < desc.BoundResources; ++i)
    {
        D3D11_SHADER_INPUT_BIND_DESC bindDesc;
        reflector->GetResourceBindingDesc(i, &bindDesc);
        std::string resourceName = bindDesc.Name;

        ShaderParameterCatetory parameterType = ShaderParameterCatetory::kUnknown;

        switch (bindDesc.Type)
        {
        case D3D_SIT_CBUFFER:
            parameterType = ShaderParameterCatetory::kCBV;
            break;
        case D3D_SIT_TEXTURE:
        case D3D_SIT_STRUCTURED:
            parameterType = ShaderParameterCatetory::kSRV;
            break;
        case D3D_SIT_SAMPLER:
            parameterType = ShaderParameterCatetory::kSampler;
            break;
        case D3D_SIT_UAV_RWSTRUCTURED:
        case D3D_SIT_UAV_RWTYPED:
            parameterType = ShaderParameterCatetory::kUAV;
            break;
        }

        // Create an empty shader parameter that should be filled-in by the application.
        params_.emplace(resourceName, ShaderParameter{
            resourceName, shader->type(), parameterType, bindDesc.BindPoint, bindDesc.BindCount });
    }
}

}
}
