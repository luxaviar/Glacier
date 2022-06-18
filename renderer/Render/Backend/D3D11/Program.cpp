#include "Program.h"
#include <d3dcompiler.h>
#include "Common/TypeTraits.h"
#include "exception/exception.h"
#include "GfxDriver.h"
#include "Texture.h"
#include "Sampler.h"
#include "Buffer.h"
#include "Render/Material.h"

namespace glacier {
namespace render {

D3D11Program::D3D11Program(const char* name) : Program(name) {

}

void D3D11Program::SetupShaderParameter(const std::shared_ptr<Shader>& shader) {
    auto& params = shader->GetAllParameters();

    for (auto& [name, param] : params) {
        for (auto& entry : param.entries) {
            if (entry) {
                SetShaderParameter(name, entry);
            }
        }
    }
}

void D3D11Program::Bind(GfxDriver* gfx, Material* mat) {
    auto& properties = mat->GetProperties();
    for (auto& [_, prop] : properties) {
        Bind(gfx, prop);
    }
}

void D3D11Program::UnBind(GfxDriver* gfx, Material* mat) {
    auto& properties = mat->GetProperties();
    for (auto& [_, prop] : properties) {
        UnBind(gfx, prop);
    }
}

void D3D11Program::BindPSO(GfxDriver* gfx) {
    if (pso_) {
        pso_->Bind(gfx);
    }

    for (auto& shader : shaders_) {
        if (shader) {
            shader->Bind();
        }
    }
}

void D3D11Program::UnBindPSO(GfxDriver* gfx) {
    for (auto& shader : shaders_) {
        if (shader) {
            shader->UnBind();
        }
    }
}

void D3D11Program::Bind(GfxDriver* gfx, const MaterialProperty& prop) {
    for (auto& shader_param : prop.shader_param->entries) {
        if (shader_param) {
            switch (prop.prop_type)
            {
            case MaterialPropertyType::kTexture:
                if ((prop.use_default && prop.dirty) || !prop.resource) {
                    auto desc = Texture::Description()
                        .SetColor(prop.color)
                        .SetFormat(TextureFormat::kR8G8B8A8_UNORM)
                        .SetDimension(8, 8);
                    prop.resource = gfx->CreateTexture(desc);
                }

                dynamic_cast<D3D11Texture*>(prop.resource.get())->Bind(shader_param.shader_type, shader_param.bind_point);
                break;
            case MaterialPropertyType::kSampler:
                if (!prop.resource || prop.dirty) {
                    prop.resource = D3D11Sampler::Create(gfx, prop.sampler_state);
                }
                dynamic_cast<D3D11Sampler*>(prop.resource.get())->Bind(shader_param.shader_type, shader_param.bind_point);
                break;
            case MaterialPropertyType::kConstantBuffer:
                dynamic_cast<D3D11ConstantBuffer*>(prop.resource.get())->Bind(shader_param.shader_type, shader_param.bind_point);
                break;
            case MaterialPropertyType::kColor:
            case MaterialPropertyType::kFloat4:
            case MaterialPropertyType::kMatrix:
                if (!prop.resource) {
                    if (prop.prop_type == MaterialPropertyType::kColor) {
                        prop.resource = gfx->CreateConstantBuffer<Color>(prop.color);
                    }
                    else if (prop.prop_type == MaterialPropertyType::kFloat4) {
                        prop.resource = gfx->CreateConstantBuffer<Vec4f>(prop.float4);
                    }
                    else {
                        prop.resource = gfx->CreateConstantBuffer<Matrix4x4>(prop.matrix);
                    }
                }
                else if (prop.dirty) {
                    //works for color & float4 & matrix
                    dynamic_cast<Buffer*>(prop.resource.get())->Update(&prop.matrix);
                }
                dynamic_cast<D3D11ConstantBuffer*>(prop.resource.get())->Bind(shader_param.shader_type, shader_param.bind_point);
                break;
            default:
                throw std::exception{ "Bad parameter type for D3D11Program::Bind." };
                break;
            }
            prop.dirty = false;
        }
    }
}

void D3D11Program::UnBind(GfxDriver* gfx, const MaterialProperty& prop) {
    for (auto& shader_param : prop.shader_param->entries) {
        if (shader_param) {
            switch (prop.prop_type)
            {
            case MaterialPropertyType::kTexture:
                dynamic_cast<D3D11Texture*>(prop.resource.get())->UnBind(shader_param.shader_type, shader_param.bind_point);
                break;
            case MaterialPropertyType::kSampler:
                dynamic_cast<D3D11Sampler*>(prop.resource.get())->UnBind(shader_param.shader_type, shader_param.bind_point);
                break;
            case MaterialPropertyType::kColor:
            case MaterialPropertyType::kFloat4:
            case MaterialPropertyType::kMatrix:
            case MaterialPropertyType::kConstantBuffer:
                dynamic_cast<D3D11ConstantBuffer*>(prop.resource.get())->UnBind(shader_param.shader_type, shader_param.bind_point);
                break;
            default:
                throw std::exception{ "Bad parameter type for D3D11Program::UnBind." };
                break;
            }
        }
    }
}

}
}
