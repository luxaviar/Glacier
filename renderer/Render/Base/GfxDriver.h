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
#include "RasterState.h"
#include "shader.h"

namespace glacier {
namespace render {

class Camera;
class Material;
class VertexData;
class InputLayout;
class InputLayoutDesc;
class IndexBuffer;
class VertexBuffer;
class ConstantBuffer;
class BufferData;
class PipelineState;
class Sampler;
struct SamplerState;
struct SamplerBuilder;
class Texture;
struct TextureBuilder;
class RenderTarget;
class Query;
class SwapChain;

class GfxDriver : private Uncopyable {
public:
    constexpr static uint32_t kBufferCount = 3;

    virtual ~GfxDriver() {}

    virtual SwapChain* GetSwapChain() const = 0;

    virtual void EndFrame() = 0;
    virtual void Present() = 0;
    virtual void BeginFrame() = 0;

    virtual void DrawIndexed(uint32_t count) = 0;
    virtual void Draw(uint32_t count, uint32_t offset) = 0;

    virtual std::shared_ptr<InputLayout> CreateInputLayout(const InputLayoutDesc& layout) = 0;
    virtual std::shared_ptr<IndexBuffer> CreateIndexBuffer(const std::vector<uint32_t>& indices) = 0;

    virtual std::shared_ptr<VertexBuffer> CreateVertexBuffer(const VertexData& vertices) = 0;
    virtual std::shared_ptr<VertexBuffer> CreateVertexBuffer(const void* data, size_t size, size_t stride, UsageType usage) = 0;

    virtual std::shared_ptr<ConstantBuffer> CreateConstantBuffer(const void* data, size_t size, UsageType usage) = 0;
    virtual std::shared_ptr<ConstantBuffer> CreateConstantBuffer(std::shared_ptr<BufferData>& data) = 0;

    template<typename T>
    std::shared_ptr<ConstantBuffer> CreateConstantBuffer(const T& data, UsageType usage = UsageType::kDefault) {
        return CreateConstantBuffer(&data, sizeof(data), usage);
    }

    template<typename T>
    std::shared_ptr<ConstantBuffer> CreateConstantBuffer(UsageType usage = UsageType::kDefault) {
        return CreateConstantBuffer(nullptr, sizeof(T), usage);
    }

    virtual std::shared_ptr<PipelineState> CreatePipelineState(RasterState rs) = 0;

    virtual std::shared_ptr<Sampler> CreateSampler(const SamplerState& ss) = 0;
    virtual std::shared_ptr<Sampler> CreateSampler(const SamplerBuilder& builder) = 0;

    virtual std::shared_ptr<Shader> CreateShader(ShaderType type, const TCHAR* file_name,
        const char* entry_point = nullptr, const char* target = nullptr,
        const std::vector<ShaderMacroEntry>& macros = { {nullptr, nullptr} }) = 0;

    virtual std::shared_ptr<Texture> CreateTexture(const TextureBuilder& builder) = 0;
    virtual std::shared_ptr<Query> CreateQuery(QueryType type, int capacity) = 0;

    virtual std::shared_ptr<RenderTarget> CreateRenderTarget(uint32_t width, uint32_t height) = 0;

    void ToggleImgui() noexcept { imgui_enable_ = !imgui_enable_; }

    void BindCamera(const Camera* cam);
    void BindCamera(const Matrix4x4& view, const Matrix4x4& projection);

    const Matrix4x4& projection() const noexcept { return projection_; }
    const Matrix4x4& view() const noexcept { return view_; }

    uint32_t width() const noexcept { return width_; }
    uint32_t height() const noexcept { return height_; }

    bool vsync() const { return vsync_; }
    void vsync(bool v) { vsync_ = v; }

    const RasterState& raster_state() const { return raster_state_; }
    void raster_state(const RasterState& rs) { raster_state_ = rs; }

    void UpdateInputLayout(const std::shared_ptr<InputLayout>& layout);
    void UpdatePipelineState(const RasterState& rs);

    bool PushMaterial(Material* mat);
    void PopMaterial(Material* mat);
    void PopMaterial(int n);

    static GfxDriver* Get() { return driver_; }

protected:
    static GfxDriver* driver_;

    bool vsync_ = false;
    uint32_t width_;
    uint32_t height_;
    Matrix4x4 view_;
    Matrix4x4 projection_;

    RasterState raster_state_;
    InputLayout* input_layout_ = nullptr;
    std::stack<Material*> materials_;

    bool imgui_enable_ = true;
};

}
}