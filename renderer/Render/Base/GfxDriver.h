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
class CommandQueue;
class CommandBuffer;

class GfxDriver : private Uncopyable {
public:
    constexpr static uint32_t kBufferCount = 3;

    virtual void Init(HWND hWnd, int width, int height, TextureFormat format) =0;
    virtual void OnDestroy() {}

    virtual ~GfxDriver() {}

    virtual SwapChain* GetSwapChain() const = 0;

    virtual void EndFrame() = 0;
    virtual void Present(CommandBuffer* cmd_buffer) = 0;
    virtual void BeginFrame() = 0;

    virtual void CheckMSAA(uint32_t sample_count, uint32_t& smaple_count, uint32_t& quality_level) = 0;

    virtual CommandQueue* GetCommandQueue(CommandBufferType type) = 0;
    virtual CommandBuffer* GetCommandBuffer(CommandBufferType type) = 0;

    virtual std::shared_ptr<Buffer> CreateIndexBuffer(size_t size, IndexFormat type) = 0;
    virtual std::shared_ptr<Buffer> CreateVertexBuffer(size_t size, size_t stride) = 0;
    virtual std::shared_ptr<Buffer> CreateConstantBuffer(const void* data, size_t size, UsageType usage = UsageType::kDynamic) = 0;

    template<typename T>
    std::shared_ptr<Buffer> CreateConstantBuffer(const T& data, UsageType usage = UsageType::kDynamic) {
        return CreateConstantBuffer(&data, sizeof(data), usage);
    }

    template<typename T>
    std::shared_ptr<Buffer> CreateConstantBuffer(UsageType usage = UsageType::kDynamic) {
        return CreateConstantBuffer(nullptr, sizeof(T), usage);
    }

    template<typename T, UsageType U>
    ConstantParameter<T> CreateConstantParameter() {
        auto cbuf = CreateConstantBuffer(nullptr, sizeof(T), U);
        return ConstantParameter<T>(cbuf);
    }

    template<typename T, UsageType U, typename ...Args>
    ConstantParameter<T> CreateConstantParameter(Args&&... args) {
        T param{ std::forward<Args>(args)... };
        auto cbuf = CreateConstantBuffer(&param, sizeof(T), U);
        return ConstantParameter<T>(cbuf, param);
    }

    virtual std::shared_ptr<PipelineState> CreatePipelineState(Program* program, RasterStateDesc rs, const InputLayoutDesc& layout) =0;

    virtual std::shared_ptr<Shader> CreateShader(ShaderType type, const TCHAR* file_name, const char* entry_point = nullptr, 
        const std::vector<ShaderMacroEntry>& macros = { {nullptr, nullptr} }, const char* target = nullptr) = 0;

    virtual std::shared_ptr<Program> CreateProgram(const char* name, const TCHAR* vs = nullptr, const TCHAR* ps = nullptr) = 0;

    virtual std::shared_ptr<Texture> CreateTexture(const TextureDescription& desc) = 0;
    virtual std::shared_ptr<Texture> CreateTexture(SwapChain* swapchain) = 0;
    virtual std::shared_ptr<Query> CreateQuery(QueryType type, int capacity) = 0;

    virtual std::shared_ptr<RenderTarget> CreateRenderTarget(uint32_t width, uint32_t height) = 0;

    void ToggleImgui() noexcept { imgui_enable_ = !imgui_enable_; }

    bool vsync() const { return vsync_; }
    void vsync(bool v) { vsync_ = v; }

    static GfxDriver* Get() { return driver_; }

protected:
    static GfxDriver* driver_;

    bool vsync_ = false;
    bool imgui_enable_ = true;
};

}
}