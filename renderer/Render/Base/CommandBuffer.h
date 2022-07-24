#pragma once

#include <vector>
#include <atomic>              // For std::atomic_bool
#include <condition_variable>  // For std::condition_variable.
#include <cstdint>             // For uint64_t
#include "Enums.h"
#include "RasterStateDesc.h"
#include "Math/Mat4.h"
#include "Math/Vec3.h"
#include "Common/Color.h"
#include "Common/Uncopyable.h"

namespace glacier {
namespace render {

class CommandQueue;
class PipelineState;
class RootSignature;
class Program;
class GfxDriver;
class Resource;
class Material;
class Camera;
class RenderTarget;
class Buffer;
class Texture;

class CommandBuffer : private Uncopyable {
public:
    CommandBuffer(GfxDriver* driver, CommandBufferType type);
    virtual ~CommandBuffer() {}

    virtual void SetName(const char* Name) = 0;
    virtual void Reset() = 0;

    virtual std::shared_ptr<Texture> CreateTextureFromFile(const TCHAR* file, bool srgb,
        bool gen_mips, TextureType type = TextureType::kTexture2D) = 0;

    virtual std::shared_ptr<Texture> CreateTextureFromColor(const Color& color, bool srgb) = 0;
    virtual void GenerateMipMaps(Texture* texture) = 0;

    bool IsClosed() const { return closed_; }
    virtual void Close() = 0;
    virtual bool Close(CommandBuffer* pending_cmd_list) = 0;

    virtual void SetGraphics32BitConstants(uint32_t root_param_index, uint32_t num_32bit_value, const void* data) = 0;
    template<typename T>
    void SetGraphics32BitConstants(uint32_t root_param_index, const T& data) {
        static_assert(sizeof(T) % sizeof(uint32_t) == 0, "Size of type must be a multiple of 4 bytes");
        SetGraphics32BitConstants(root_param_index, sizeof(T) / sizeof(uint32_t), &data);
    }

    virtual void SetCompute32BitConstants(uint32_t root_param_index, uint32_t num_32bit_value, const void* data) = 0;
    template<typename T>
    void SetCompute32BitConstants(uint32_t root_param_index, const T& data) {
        static_assert(sizeof(T) % sizeof(uint32_t) == 0, "Size of type must be a multiple of 4 bytes");
        SetCompute32BitConstants(root_param_index, sizeof(T) / sizeof(uint32_t), &data);
    }

    virtual void SetConstantBufferView(uint32_t root_param_index, const Resource* res) = 0;
    virtual void SetShaderResourceView(uint32_t root_param_index, const Resource* res) = 0;
    virtual void SetUnorderedAccessView(uint32_t root_param_index, const Resource* res) = 0;

    virtual void SetDescriptorTable(uint32_t root_param_index, uint32_t offset, const Resource* res, bool uav=false) = 0;
    virtual void SetSamplerTable(uint32_t root_param_index, uint32_t offset, const Resource* res) = 0;

    virtual void TransitionBarrier(Resource* res, ResourceAccessBit after_state,
        uint32_t subresource = BARRIER_ALL_SUBRESOURCES) = 0;

    virtual void AliasResource(Resource* before, Resource* after) = 0;
    virtual void UavResource(Resource* res) = 0;

    virtual void CopyResource(Resource* src, Resource* dst) = 0;

    virtual void DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
        INT BaseVertexLocation, UINT StartInstanceLocation) = 0;
    virtual void DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) = 0;

    virtual void Dispatch(uint32_t thread_group_x, uint32_t thread_group_y, uint32_t thread_group_z) = 0;

    virtual void FlushBarriers() = 0;
    virtual void BindDescriptorHeaps() = 0;

    virtual CommandBuffer* GetGenerateMipsCommandList() const { return compute_cmd_buffer_; }

    void BindMaterial(Material* mat);
    void UnBindMaterial();

    void BindCamera(const Camera* cam);
    void BindCamera(const Vector3& pos, const Matrix4x4& view, const Matrix4x4& projection);

    const Vector3& camera_position() const { return camera_position_; }
    const Matrix4x4& projection() const noexcept { return projection_; }
    const Matrix4x4& view() const noexcept { return view_; }

    void SetCurrentRenderTarget(std::shared_ptr<RenderTarget> rt) { current_render_target_ = rt; }
    const std::shared_ptr<RenderTarget>& GetCurrentRenderTarget() const { return current_render_target_; }

    GfxDriver* GetDriver() const { return driver_; }

protected:
    bool closed_ = false;
    
    GfxDriver* driver_;
    CommandBufferType type_;

    CommandBuffer* compute_cmd_buffer_ = nullptr;

    Matrix4x4 view_;
    Matrix4x4 projection_;
    Vector3 camera_position_;
    std::shared_ptr<RenderTarget> current_render_target_;

    Material* material_ = nullptr;
    Program* program_ = nullptr;
};

}
}
