#include "Program.h"
#include <d3d12shader.h>
#include <d3dcompiler.h>
#include "Common/TypeTraits.h"
#include "exception/exception.h"
#include "GfxDriver.h"
#include "Buffer.h"
#include "Texture.h"
#include "Render/Material.h"
#include "RenderTarget.h"
#include "PipelineState.h"
#include "Sampler.h"
#include "Inspect/Profiler.h"

namespace glacier {
namespace render {

D3D12Program::D3D12Program(const char* name) : Program(name) {

}

ID3D12RootSignature* D3D12Program::GetRootSignature() {
    if (!root_signature_) {
        CreateRootSignature();
    }

    return root_signature_.Get();
}

void D3D12Program::Bind(GfxDriver* gfx, Material* mat) {
    auto driver = static_cast<D3D12GfxDriver*>(gfx);
    auto cmd_list = driver->GetCommandList();

    auto& properties = mat->GetProperties();
    for (auto& [_, prop] : properties) {
        BindProperty(gfx, cmd_list, prop);
    }

    Bind(cmd_list);
}

void D3D12Program::RefreshDynamicBuffer(GfxDriver* gfx) {
    if (!dynamic_buffer_) return;

    auto driver = static_cast<D3D12GfxDriver*>(gfx);
    auto cmd_list = driver->GetCommandList();

    for (uint32_t i = 0; i < cbv_def_.size(); ++i) {
        const auto& param = cbv_def_[i];
        assert(param.constant_buffer);

        if (param.constant_buffer->IsDynamic()) {
            if (is_compute_) {
                cmd_list->SetComputeRootConstantBufferView(cbv_def_.root_slot + i, param.constant_buffer);
            }
            else {
                cmd_list->SetGraphicsRootConstantBufferView(cbv_def_.root_slot + i, param.constant_buffer);
            }
        }
    }
}

void D3D12Program::BindPSO(GfxDriver* gfx) {
    if (pso_) {
        auto d3d12pso = static_cast<D3D12PipelineState*>(pso_.get());
        d3d12pso->Check(gfx, this);
        pso_->Bind(gfx);
    }
}

void D3D12Program::BindProperty(GfxDriver* gfx, D3D12CommandList* cmd_list, const MaterialProperty& prop) {
    for (auto& shader_param : *prop.shader_param) {
        if (shader_param) {
            auto name = shader_param.name.c_str();
            switch (prop.prop_type)
            {
            case MaterialPropertyType::kTexture:
            {
                if ((prop.use_default && prop.dirty) || !prop.texture) {
                    auto desc = Texture::Description()
                        .SetColor(prop.color)
                        .SetFormat(TextureFormat::kR8G8B8A8_UNORM)
                        .SetDimension(8, 8);
                    prop.texture = gfx->CreateTexture(desc);
                    prop.texture->SetName(TEXT("color texture"));
                }

                auto tex = static_cast<D3D12Texture*>(prop.texture.get());
                if (is_compute_) {
                    cmd_list->TransitionBarrier(tex, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                }
                else {
                    cmd_list->TransitionBarrier(tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                }

                SetParameter(name, tex);
            }
            break;
            case MaterialPropertyType::kSampler:
            {
                if (!prop.sampler) {
                    prop.sampler = D3D12Sampler::Create(gfx, prop.sampler_state);
                }


                auto sampler = static_cast<D3D12Sampler*>(prop.sampler.get());
                SetParameter(name, sampler);
            }
            break;
            case MaterialPropertyType::kConstantBuffer:
            {
                auto buf = static_cast<D3D12ConstantBuffer*>(prop.buffer.get());
                SetParameter(name, buf);
            }
            break;
            case MaterialPropertyType::kStructuredBuffer:
            {

                auto buf = static_cast<D3D12StructuredBuffer*>(prop.buffer.get());
                SetParameter(name, buf);
            }
            break;
            case MaterialPropertyType::kColor:
            case MaterialPropertyType::kFloat4:
            case MaterialPropertyType::kMatrix:
            {
                if (!prop.buffer) {
                    if (prop.prop_type == MaterialPropertyType::kColor) {
                        prop.buffer = gfx->CreateConstantBuffer<Color>(prop.color, UsageType::kDefault);
                    }
                    else if (prop.prop_type == MaterialPropertyType::kFloat4) {
                        prop.buffer = gfx->CreateConstantBuffer<Vec4f>(prop.float4, UsageType::kDefault);
                    }
                    else {
                        prop.buffer = gfx->CreateConstantBuffer<Matrix4x4>(prop.matrix, UsageType::kDefault);
                    }
                }
                else if (prop.dirty) {
                    prop.buffer->Update(&prop.matrix); //compatible with color & float4
                }

                auto cbuffer = static_cast<D3D12ConstantBuffer*>(prop.buffer.get());
                SetParameter(name, cbuffer);
            }
            break;
            default:
                throw std::exception{ "Bad parameter type for D3D12Program::Bind." };
                break;
            }
            prop.dirty = false;
        }
    }
}

void D3D12Program::SetupShaderParameter(const std::shared_ptr<Shader>& shader) {
    ID3DBlob* blob = shader->GetBytecode();
    ShaderType type = shader->type();

    is_compute_ = type == ShaderType::kCompute;

    ID3D12ShaderReflection* reflection = nullptr;
    D3DReflect(blob->GetBufferPointer(), blob->GetBufferSize(), IID_ID3D12ShaderReflection, (void**)&reflection);

    D3D12_SHADER_DESC shader_desc;
    reflection->GetDesc(&shader_desc);

    for (UINT i = 0; i < shader_desc.BoundResources; i++) {
        D3D12_SHADER_INPUT_BIND_DESC resource_desc;
        reflection->GetResourceBindingDesc(i, &resource_desc);

        auto param_name = resource_desc.Name;
        auto resource_type = resource_desc.Type;
        auto register_space = resource_desc.Space;
        auto bind_point = resource_desc.BindPoint;
        auto bind_count = resource_desc.BindCount;

        D3D12ShaderParameter param;
        switch (resource_type) 
        {
        case D3D_SHADER_INPUT_TYPE::D3D_SIT_CBUFFER:
            param.name = param_name;
            param.shader_type = type;
            param.bind_point = bind_point;
            param.register_space = register_space;
            param.category = ShaderParameterCatetory::kCBV;
            AddParameter(cbv_def_, param);
            break;
        case D3D_SHADER_INPUT_TYPE::D3D_SIT_STRUCTURED:
        case D3D_SHADER_INPUT_TYPE::D3D_SIT_TEXTURE:
            param.name = param_name;
            param.shader_type = type;
            param.bind_point = bind_point;
            param.bind_count = bind_count;
            param.register_space = register_space;
            param.category = ShaderParameterCatetory::kSRV;
            AddParameter(srv_def_, param);
            break;
        case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWSTRUCTURED:
        case D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWTYPED:
            assert(type == ShaderType::kCompute);
            param.name = param_name;
            param.shader_type = type;
            param.bind_point = bind_point;
            param.bind_count = bind_count;
            param.register_space = register_space;
            param.category = ShaderParameterCatetory::kUAV;
            AddParameter(uav_def_, param);
            break;
        case D3D_SHADER_INPUT_TYPE::D3D_SIT_SAMPLER:
            assert(type == ShaderType::kPixel);
            param.name = param_name;
            param.shader_type = type;
            param.bind_point = bind_point;
            param.register_space = register_space;
            param.category = ShaderParameterCatetory::kSampler;
            AddParameter(sampler_def_, param);
            break;
        }

        SetShaderParameter(param.name, ShaderParameter{ param.name, param.shader_type, param.category, param.bind_point, param.bind_count });
    }
}

void D3D12Program::AddParameter(ParameterBinding& list, const D3D12ShaderParameter& param) {
    for (const auto& p : list.params) {
        assert(p.name != param.name || p.shader_type != param.shader_type);
    }

    list.params.push_back(param);
    list.bind_count += param.bind_count;
}

void D3D12Program::CreateRootSignature() {
    std::vector<CD3DX12_ROOT_PARAMETER> root_params;
    
    // CBV
    if (cbv_def_) {
        cbv_def_.root_slot = (uint32_t)root_params.size();
        for (auto& param : cbv_def_.params)
        {
            CD3DX12_ROOT_PARAMETER RootParam;
            RootParam.InitAsConstantBufferView(param.bind_point, param.register_space, GetShaderVisibility(param.shader_type));
            root_params.push_back(RootParam);
        }
    }

    // SRV
    std::vector<CD3DX12_DESCRIPTOR_RANGE> srv_table;
    if (srv_def_) {
        srv_def_.root_slot = (uint32_t)root_params.size();

        srv_table.resize(srv_def_.params.size());
        for (size_t i = 0; i < srv_def_.params.size(); ++i) {
            auto& param = srv_def_.params[i];
            srv_table[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, param.bind_count, param.bind_point, param.register_space);
        }

        CD3DX12_ROOT_PARAMETER RootParam;
        D3D12_SHADER_VISIBILITY ShaderVisibility = is_compute_ ? D3D12_SHADER_VISIBILITY_ALL : D3D12_SHADER_VISIBILITY_PIXEL;
        RootParam.InitAsDescriptorTable((uint32_t)srv_table.size(), srv_table.data(), ShaderVisibility);
        root_params.push_back(RootParam);
    }

    // UAV
    std::vector<CD3DX12_DESCRIPTOR_RANGE> uav_table;
    if (uav_def_) {
        uav_def_.root_slot = (uint32_t)root_params.size();

        uav_table.resize(uav_def_.params.size());
        for (size_t i = 0; i < uav_def_.params.size(); ++i) {
            auto& param = uav_def_.params[i];
            uav_table[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, param.bind_count, param.bind_point, param.register_space);
        }

        CD3DX12_ROOT_PARAMETER RootParam;
        D3D12_SHADER_VISIBILITY ShaderVisibility = is_compute_ ? D3D12_SHADER_VISIBILITY_ALL : D3D12_SHADER_VISIBILITY_PIXEL;
        RootParam.InitAsDescriptorTable((uint32_t)uav_table.size(), uav_table.data(), ShaderVisibility);
        root_params.push_back(RootParam);
    }

    std::vector<CD3DX12_DESCRIPTOR_RANGE> sampler_table;
    if (sampler_def_) {
        sampler_def_.root_slot = (uint32_t)root_params.size();

        sampler_table.resize(sampler_def_.params.size());
        for (size_t i = 0; i < sampler_def_.params.size(); ++i) {
            auto& param = sampler_def_.params[i];
            sampler_table[i].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, param.bind_count, param.bind_point, param.register_space);
        }

        CD3DX12_ROOT_PARAMETER RootParam;
        D3D12_SHADER_VISIBILITY ShaderVisibility = is_compute_ ? D3D12_SHADER_VISIBILITY_ALL : D3D12_SHADER_VISIBILITY_PIXEL;
        RootParam.InitAsDescriptorTable((uint32_t)sampler_table.size(), sampler_table.data(), ShaderVisibility);
        root_params.push_back(RootParam);
    }

    //auto StaticSamplers = CreateStaticSamplers();

    //------------------------------------------------SerializeRootSignature---------------------------------------

    CD3DX12_ROOT_SIGNATURE_DESC sig_desc((UINT)root_params.size(), root_params.data(),
        0, nullptr,
        //(UINT)StaticSamplers.size(), StaticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> sig_blob = nullptr;
    ComPtr<ID3DBlob> err_blob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&sig_desc, D3D_ROOT_SIGNATURE_VERSION_1,
        sig_blob.GetAddressOf(), err_blob.GetAddressOf());

    if (err_blob != nullptr)
    {
        ::OutputDebugStringA((char*)err_blob->GetBufferPointer());
        err_blob->Release();
    }

    GfxThrowCheck(hr);

    GfxThrowIfFailed(D3D12GfxDriver::Instance()->GetDevice()->CreateRootSignature(
        0,
        sig_blob->GetBufferPointer(),
        sig_blob->GetBufferSize(),
        IID_PPV_ARGS(&root_signature_)));
}

void D3D12Program::SetParameter(const char* name, D3D12ConstantBuffer* cbuffer) {
    for (auto& param : cbv_def_.params) {
        if (param.name == name) {
            param.constant_buffer = cbuffer;
            if (cbuffer->IsDynamic()) {
                dynamic_buffer_ = true;
            }
        }
    }
}

void D3D12Program::SetParameter(const char* name, D3D12StructuredBuffer* sbuffer) {
    for (auto& param : srv_def_.params) {
        if (param.name == name) {
            param.srv_list.clear();
            param.srv_list.push_back(&sbuffer->GetDescriptorSlot());
        }
    }
}

void D3D12Program::SetParameter(const char* name, D3D12Texture* tex) {
    for (auto& param : srv_def_.params) {
        if (param.name == name) {
            param.srv_list.clear();
            param.srv_list.push_back(&tex->GetSrvDescriptorSlot());
        }
    }
}

void D3D12Program::SetParameter(const char* name, D3D12Sampler* sampler) {
    for (auto& param : sampler_def_.params) {
        if (param.name == name) {
            param.srv_list.clear();
            param.srv_list.push_back(&sampler->GetDescriptorSlot());
        }
    }
}

void D3D12Program::SetParameter(const char* name, const D3D12DescriptorRange& descriptor, ShaderParameterCatetory category) {
    if (category == ShaderParameterCatetory::kSRV) {
        for (auto& param : srv_def_.params) {
            if (param.name == name) {
                param.srv_list.clear();
                param.srv_list.push_back(&descriptor);
            }
        }
    }
    else if (category == ShaderParameterCatetory::kUAV) {
        for (auto& param : uav_def_.params) {
            if (param.name == name) {
                param.srv_list.clear();
                param.srv_list.push_back(&descriptor);
            }
        }
    }
}

void D3D12Program::Bind(D3D12CommandList* cmd_list) {
    auto srv_table = D3D12GfxDriver::Instance()->GetSrvUavTableHeap();
    for (uint32_t i = 0; i < cbv_def_.size(); ++i) {
        const auto& param = cbv_def_[i];
        assert(param.constant_buffer);

        if (is_compute_) {
            cmd_list->SetComputeRootConstantBufferView(cbv_def_.root_slot + i, param.constant_buffer);
        }
        else {
            cmd_list->SetGraphicsRootConstantBufferView(cbv_def_.root_slot + i, param.constant_buffer);
        }
    }

    if (srv_def_) {
        auto& bindings = srv_def_.bindings;
        bindings.clear();

        for (auto& param : srv_def_.params) {
            for (uint32_t i = 0; i < param.srv_list.size(); ++i) {
                bindings.push_back(param.srv_list[i]->GetDescriptorHandle());
            }
        }

        auto gpu_address = srv_table->AppendDescriptors(bindings.data(), bindings.size());
        if (is_compute_) {
            cmd_list->SetComputeRootDescriptorTable(srv_def_.root_slot, gpu_address);
        }
        else {
            cmd_list->SetGraphicsRootDescriptorTable(srv_def_.root_slot, gpu_address);
        }
    }

    if (uav_def_) {
        auto& uav_bindings = uav_def_.bindings;
        uav_bindings.clear();

        for (auto& param : uav_def_.params) {
            for (uint32_t i = 0; i < param.srv_list.size(); ++i) {
                uav_bindings.push_back(param.srv_list[i]->GetDescriptorHandle());
            }
        }

        auto gpu_address = srv_table->AppendDescriptors(uav_bindings.data(), uav_bindings.size());

        if (is_compute_) {
            cmd_list->SetComputeRootDescriptorTable(uav_def_.root_slot, gpu_address);
        }
        else {
            cmd_list->SetGraphicsRootDescriptorTable(uav_def_.root_slot, gpu_address);
        }
    }

    if (sampler_def_) {
        auto& sampler_bindings = sampler_def_.bindings;
        sampler_bindings.clear();

        for (auto& param : sampler_def_.params) {
            for (uint32_t i = 0; i < param.srv_list.size(); ++i) {
                sampler_bindings.push_back(param.srv_list[i]->GetDescriptorHandle());
            }
        }

        auto sampler_table = D3D12GfxDriver::Instance()->GetSamplerTableHeap();
        auto gpu_address = sampler_table->AppendDescriptors(sampler_bindings.data(), sampler_bindings.size());

        if (is_compute_) {
            cmd_list->SetComputeRootDescriptorTable(sampler_def_.root_slot, gpu_address);
        }
        else {
            cmd_list->SetGraphicsRootDescriptorTable(sampler_def_.root_slot, gpu_address);
        }
    }
}

}
}
