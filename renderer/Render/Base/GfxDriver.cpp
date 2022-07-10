#include "gfxdriver.h"
#include "render/camera.h"
#include "render/material.h"
#include "inputlayout.h"
#include "pipelinestate.h"
#include "Program.h"
#include "../Material.h"

namespace glacier {
namespace render {

GfxDriver* GfxDriver::driver_ = nullptr;

}
}
