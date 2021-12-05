#include "MeshDrawer.h"
#include "Render/Mesh/MeshRenderer.h"
#include "Render/Editor/Gizmos.h"

namespace glacier {

void MeshDrawer::OnDrawGizmos() {
    auto mr = game_object_->GetComponent<render::MeshRenderer>();
    render::Gizmos::Instance()->DrawWireMesh(*mr);

}

}