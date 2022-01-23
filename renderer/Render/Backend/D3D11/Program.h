#pragma once

#include <unordered_map>
#include <vector>
#include "Shader.h"
#include "Render/Base/Program.h"
#include "Sampler.h"

namespace glacier {
namespace render {

class GfxDriver;
struct MaterialProperty;

class D3D11Program : public Program {
public:
    D3D11Program(const char* name);

    void Bind(GfxDriver* gfx, Material* mat) override;
    void UnBind(GfxDriver* gfx, Material* mat) override;

    void Bind(GfxDriver* gfx, MaterialTemplate* mat) override;
    void UnBind(GfxDriver* gfx, MaterialTemplate* mat) override;

protected:
    //void BindSampler();

    void Bind(GfxDriver* gfx, const MaterialProperty& prop);
    void UnBind(GfxDriver* gfx, const MaterialProperty& prop);

    void SetupShaderParameter(const std::shared_ptr<Shader>& shader) override;

    //std::vector<std::shared_ptr<D3D11Sampler>> samplers_;
};

}
}
