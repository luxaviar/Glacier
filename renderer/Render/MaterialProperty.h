#pragma once

#include <memory>
#include <variant>
#include "Common/Color.h"
#include "Render/Base/Shader.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/Texture.h"
#include "Render/Base/SamplerState.h"

namespace glacier {
namespace render {

class GfxDriver;

struct MaterialProperty {
    MaterialProperty(const ShaderParameterSet* param, const std::shared_ptr<Texture>& tex,
        const Color& default_color);

    MaterialProperty(const ShaderParameterSet* param, const std::shared_ptr<Buffer>& buf);
    MaterialProperty(const ShaderParameterSet* param, const Color& color);
    MaterialProperty(const ShaderParameterSet* param, const Vec4f& float4);
    MaterialProperty(const ShaderParameterSet* param, const Matrix4x4& matrix);
    MaterialProperty(const ShaderParameterSet* param, const SamplerState& ss);

    MaterialProperty(const MaterialProperty& other) noexcept;
    MaterialProperty(MaterialProperty&& other) noexcept;

    mutable bool dirty = false;
    const ShaderParameterSet* shader_param;
    MaterialPropertyType prop_type;

    //texture
    bool use_default = false;
    mutable std::shared_ptr<Texture> texture;
    mutable std::shared_ptr<Buffer> buffer;
    mutable std::shared_ptr<Sampler> sampler;

    union {
        Color default_color; //texture color
        Color color;
        Vec4f float4;
        Matrix4x4 matrix;
        SamplerState sampler_state;
    };
};

}
}
