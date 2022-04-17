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
        SetShaderParameter(name, param);
    }
}

void D3D11Program::Bind(GfxDriver* gfx, Material* mat) {
    auto& properties = mat->GetProperties();
    for (auto& prop : properties) {
        Bind(gfx, prop);
    }
}

void D3D11Program::UnBind(GfxDriver* gfx, Material* mat) {
    auto& properties = mat->GetProperties();
    for (auto& prop : properties) {
        UnBind(gfx, prop);
    }
}

void D3D11Program::Bind(GfxDriver* gfx, MaterialTemplate* mat) {
    if (pso_) {
        pso_->Bind(gfx);
    }

    for (auto& shader : shaders_) {
        if (shader) {
            shader->Bind();
        }
    }

    //BindSampler();

    auto& properties = mat->GetProperties();
    for (auto& prop : properties) {
        Bind(gfx, prop);
    }
}

void D3D11Program::UnBind(GfxDriver* gfx, MaterialTemplate* mat) {
    for (auto& shader : shaders_) {
        if (shader)
            shader->UnBind();
    }

    auto& properties = mat->GetProperties();
    for (auto& prop : properties) {
        UnBind(gfx, prop);
    }
}

void D3D11Program::Bind(GfxDriver* gfx, const MaterialProperty& prop) {
    for (auto& shader_param : *prop.shader_param) {
        if (shader_param) {
            switch (prop.prop_type)
            {
            case MaterialPropertyType::kTexture:
                if ((prop.use_default && prop.dirty) || !prop.texture) {
                    auto desc = Texture::Description()
                        .SetColor(prop.color)
                        .SetFormat(TextureFormat::kR8G8B8A8_UNORM)
                        .SetDimension(8, 8);
                    prop.texture = gfx->CreateTexture(desc);
                }

                static_cast<D3D11Texture*>(prop.texture.get())->Bind(shader_param.shader_type, shader_param.bind_point);
                break;
            case MaterialPropertyType::kSampler:
                if (!prop.sampler) {
                    prop.sampler = D3D11Sampler::Create(gfx, prop.sampler_state);
                }
                static_cast<D3D11Sampler*>(prop.sampler.get())->Bind(shader_param.shader_type, shader_param.bind_point);
                break;
            case MaterialPropertyType::kConstantBuffer:
                static_cast<D3D11ConstantBuffer*>(prop.buffer.get())->Bind(shader_param.shader_type, shader_param.bind_point);
                break;
            case MaterialPropertyType::kColor:
            case MaterialPropertyType::kFloat4:
            case MaterialPropertyType::kMatrix:
                if (!prop.buffer) {
                    if (prop.prop_type == MaterialPropertyType::kColor) {
                        prop.buffer = gfx->CreateConstantBuffer<Color>(prop.color);
                    }
                    else if (prop.prop_type == MaterialPropertyType::kFloat4) {
                        prop.buffer = gfx->CreateConstantBuffer<Vec4f>(prop.float4);
                    }
                    else {
                        prop.buffer = gfx->CreateConstantBuffer<Matrix4x4>(prop.matrix);
                    }
                }
                else if (prop.dirty) {
                    prop.buffer->Update(&prop.matrix); //it also works for color & float4
                }
                static_cast<D3D11ConstantBuffer*>(prop.buffer.get())->Bind(shader_param.shader_type, shader_param.bind_point);
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
    for (auto& shader_param : *prop.shader_param) {
        if (shader_param) {
            switch (prop.prop_type)
            {
            case MaterialPropertyType::kTexture:
                static_cast<D3D11Texture*>(prop.texture.get())->UnBind(shader_param.shader_type, shader_param.bind_point);
                break;
            case MaterialPropertyType::kSampler:
                static_cast<D3D11Sampler*>(prop.sampler.get())->UnBind(shader_param.shader_type, shader_param.bind_point);
                break;
            case MaterialPropertyType::kColor:
            case MaterialPropertyType::kFloat4:
            case MaterialPropertyType::kMatrix:
            case MaterialPropertyType::kConstantBuffer:
                static_cast<D3D11ConstantBuffer*>(prop.buffer.get())->UnBind(shader_param.shader_type, shader_param.bind_point);
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
