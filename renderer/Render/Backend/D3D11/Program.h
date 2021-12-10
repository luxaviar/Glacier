#pragma once

#include <unordered_map>
#include <vector>
#include "Shader.h"
#include "Render/Base/Program.h"

namespace glacier {
namespace render {

class D3D11Program : public Program {
public:
    D3D11Program(const char* name);
    //void SetShader(std::shared_ptr<DX12Shader>& shader);
    
protected:
    void SetupShaderParameter(const std::shared_ptr<Shader>& shader) override;
};

}
}
