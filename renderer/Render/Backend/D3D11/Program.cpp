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
    auto& params = sh->GetAllParameters();

    for (auto& [name, param] : params) {
        params_.emplace(name, param);
    }
}

}
}
