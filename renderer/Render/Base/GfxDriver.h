#pragma once

#include <vector>
#include <stack>
#include <memory>
#include <random>
#include "Common/Util.h"
#include "Math/Mat4.h"
#include "Common/Uncopyable.h"
#include "Common/Singleton.h"
#include "common/signal.h"
#include "enums.h"
#include "RasterStateDesc.h"
#include "shader.h"
#include "Buffer.h"

namespace glacier {
namespace render {

class Camera;
class Material;
class VertexData;
class InputLayout;
class InputLayoutDesc;
class Buffer;
class BufferData;
class Sampler;
struct SamplerState;
class Texture;
struct TextureDescription;
class RenderTarget;
class Query;
class SwapChain;
class Program;
class PipelineState;
class MaterialTemplate;

class GfxDriver : private Uncopyable {
public:
    constexpr static uint32_t kBufferCount = 3;

    virtual void Init(HWND hWnd, int width, int height, TextureFormat format) =0;
    virtual void OnDestroy() {}

    virtual ~GfxDriver() {}

    virtual SwapChain* GetSwapChain() const = 0;

    virtual void EndFrame() = 0;
    virtual void Present() = 0;
    virtual void BeginFrame() = 0;

    virtual void CheckMSAA(MSAAType msaa, uint32_t& smaple_count, uint32_t& quality_level) = 0;

    virtual void DrawIndexed(uint32_t count) = 0;
    virtual void Draw(uint32_t count, uint32_t offset) = 0;

    virtual void CopyResource(const std::shared_ptr<Resource>& src, const std::shared_ptr<Resource>& dst) = 0;

    virtual std::shared_ptr<IndexBuffer> CreateIndexBuffer(const std::vector<uint32_t>& indices) = 0;
    virtual std::shared_ptr<IndexBuffer> CreateIndexBuffer(const void* data, size_t size, IndexFormat type, UsageType usage) = 0;

    virtual std::shared_ptr<VertexBuffer> CreateVertexBuffer(const VertexData& vertices) = 0;
    virtual std::shared_ptr<VertexBuffer> CreateVertexBuffer(const void* data, size_t size, size_t stride, UsageType usage) = 0;

    virtual std::shared_ptr<ConstantBuffer> CreateConstantBuffer(const void* data, size_t size, UsageType usage = UsageType::kDynamic) = 0;
    virtual std::shared_ptr<ConstantBuffer> CreateConstantBuffer(std::shared_ptr<BufferData>& data, UsageType usage = UsageType::kDynamic) = 0;

    template<typename T>
    std::shared_ptr<ConstantBuffer> CreateConstantBuffer(const T& data, UsageType usage = UsageType::kDynamic) {
        return CreateConstantBuffer(&data, sizeof(data), usage);
    }

    template<typename T>
    std::shared_ptr<ConstantBuffer> CreateConstantBuffer(UsageType usage = UsageType::kDynamic) {
        return CreateConstantBuffer(nullptr, sizeof(T), usage);
    }

    template<typename T>
    ConstantParameter<T> CreateConstantParameter(UsageType usage = UsageType::kDynamic) {
        auto cbuf = CreateConstantBuffer(nullptr, sizeof(T), usage);
        return ConstantParameter<T>(cbuf);
    }

    virtual std::shared_ptr<PipelineState> CreatePipelineState(RasterStateDesc rs, const InputLayoutDesc& layout) =0;

    virtual std::shared_ptr<Shader> CreateShader(ShaderType type, const TCHAR* file_name, const char* entry_point = nullptr, 
        const std::vector<ShaderMacroEntry>& macros = { {nullptr, nullptr} }, const char* target = nullptr) = 0;

    virtual std::shared_ptr<Program> CreateProgram(const char* name, const TCHAR* vs = nullptr, const TCHAR* ps = nullptr) = 0;

    virtual std::shared_ptr<Texture> CreateTexture(const TextureDescription& desc) = 0;
    virtual std::shared_ptr<Texture> CreateTexture(SwapChain* swapchain) = 0;
    virtual std::shared_ptr<Query> CreateQuery(QueryType type, int capacity) = 0;

    virtual std::shared_ptr<RenderTarget> CreateRenderTarget(uint32_t width, uint32_t height) = 0;

    void ToggleImgui() noexcept { imgui_enable_ = !imgui_enable_; }

    void BindCamera(const Camera* cam);
    void BindCamera(const Vector3& pos, const Matrix4x4& view, const Matrix4x4& projection);

    const Vector3& camera_position() const { return camera_position_; }
    const Matrix4x4& projection() const noexcept { return projection_; }
    const Matrix4x4& view() const noexcept { return view_; }

    bool vsync() const { return vsync_; }
    void vsync(bool v) { vsync_ = v; }

    const RasterStateDesc& raster_state() const { return raster_state_; }
    void raster_state(const RasterStateDesc& rs) { raster_state_ = rs; }

    const uint32_t& layout_signature() const { return input_layout_; }
    void layout_signature(const uint32_t& sig) { input_layout_ = sig; }

    virtual void BindMaterial(Material* mat) = 0;
    virtual void UnBindMaterial() = 0;

    virtual void Flush() {}

    static GfxDriver* Get() { return driver_; }

protected:
    static GfxDriver* driver_;

    bool vsync_ = false;
    Vector3 camera_position_;
    Matrix4x4 view_;
    Matrix4x4 projection_;

    RasterStateDesc raster_state_;
    uint32_t input_layout_ = { 0 };
    Material* material_ = nullptr;
    MaterialTemplate* material_template_ = nullptr;

    bool imgui_enable_ = true;
};

}
}