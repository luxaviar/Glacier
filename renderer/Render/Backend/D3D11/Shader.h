#pragma once

#include <d3d11_1.h>
#include <unordered_map>
#include "render/base/shader.h"

namespace glacier {
namespace render {

class D3D11Shader : public Shader {
public:
    static HRESULT CompileFromFile(const TCHAR* file_path, ID3DBlob** ptr_blob, const D3D_SHADER_MACRO* defines,
        const char* entry_point, const char* target, UINT flags1=0, UINT flags2=0);

    static std::shared_ptr<D3D11Shader> Create(ShaderType type, const TCHAR* file_name, const char* entry_point,
        const char* target);

    static void Clear() { cache_.clear(); }

    D3D11Shader(ShaderType type, const TCHAR* file_name, const char* entry_point = nullptr,
        const char* target = nullptr, const std::vector<ShaderMacroEntry>& macros = { {0,0} });
    
    void Bind() override;
    void UnBind() override;

    ID3DBlob* GetBytecode() const noexcept;

protected:
    static const char* GetLatestTarget(ShaderType type);

    void SetupParameter();

    ComPtr<ID3DBlob> blob_;
    ComPtr<ID3D11VertexShader> vertex_shader_;
    ComPtr<ID3D11HullShader> hull_shader_;
    ComPtr<ID3D11DomainShader> domain_shader_;
    ComPtr<ID3D11GeometryShader> geometry_shader_;
    ComPtr<ID3D11PixelShader> pixel_shader_;
    ComPtr<ID3D11ComputeShader> compute_shader_;

    static std::unordered_map<std::string, std::shared_ptr<D3D11Shader>> cache_;
};

}
}
