#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
#include "Common/Singleton.h"
#include "Render/Base/Buffer.h"
#include "Render/Base/GfxDriver.h"

namespace glacier {
namespace render {

class Mesh;
class Material;
class CommandBuffer;

//Deforms the bind pose of a skinned mesh on the GPU into the vertices the
//passes draw, so the mesh is skinned once per frame instead of once per pass,
//and the vertex shader of a pass only transforms what it is given.
//
//The skinned vertices of every mesh live in one buffer, one region per mesh, so
//there is a single unordered access view for the whole frame. A mesh that does
//not fit keeps the vertex shader skinning path.
class GpuSkinning : public Singleton<GpuSkinning> {
public:
    //vertices of all skinned meshes of the scene, 65536 of them are 3.5MB
    static constexpr uint32_t kCapacity = 65536;
    static constexpr uint32_t kInvalidOffset = 0xffffffff;
    //vertices one thread group skins, must match SkinningCS.hlsl
    static constexpr uint32_t kThreads = 64;

    void Setup();
    void Release();

    //region of the pool this mesh is skinned into, kInvalidOffset when it does
    //not fit; the mesh keeps the region until it is released
    uint32_t Allocate(const Mesh* mesh);
    void Free(uint32_t offset);

    //skins the vertices of a mesh of one instance of it; has to run before the
    //passes that draw it. bone_offset and prev_bone_offset are the slots the
    //instance was given in the bone matrix pool.
    void Dispatch(CommandBuffer* cmd_buffer, const Mesh* mesh, uint32_t vertex_offset,
        uint32_t bone_offset, uint32_t prev_bone_offset);

    //the buffer the skinned vertices live in, which the passes draw from
    const std::shared_ptr<Buffer>& vertex_buffer() const { return vertex_buffer_; }

private:
    //must match _SkinningParams in SkinningCS.hlsl
    struct SkinParams {
        uint32_t vertex_count;
        //region of this mesh in the pool, in vertices
        uint32_t vertex_offset;
        //slot of the pose of this frame and of the one before it in _BoneMatrices
        uint32_t bone_offset;
        uint32_t prev_bone_offset;
    };

    std::shared_ptr<Buffer> vertex_buffer_;
    std::shared_ptr<Material> material_;
    ConstantParameter<SkinParams> skin_params_;
    //regions handed out, in the order they were given to meshes; a mesh keeps
    //its region until it dies
    std::vector<std::pair<uint32_t, uint32_t>> regions_;
};

}
}
