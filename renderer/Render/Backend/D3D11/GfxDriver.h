#pragma once

#include <d3d11_1.h>
#include <d3d11sdklayers.h>
#include <dxgi1_3.h>
#include "Common/Util.h"
#include "render/base/gfxdriver.h"
#include "SwapChain.h"

#pragma comment(lib,"d3d11.lib")

namespace glacier {
namespace render {

class D3D11SwapChain;

class GfxDriverD3D11 : public GfxDriver, public Singleton<GfxDriverD3D11> {
public:
    ~GfxDriverD3D11();

    void Init(HWND hWnd, int width, int height);

    SwapChain* GetSwapChain() const override { return swap_chain_.get(); }
    D3D11SwapChain* GetUnderlyingSwapChain() const { return swap_chain_.get(); }

    ID3D11Device* GetDevice() { return device_.Get(); }
    ID3D11DeviceContext* GetContext() { return context_.Get(); }

    void EndFrame() override;
    void Present() override;
    void BeginFrame() override;

    void DrawIndexed(uint32_t count) override;
    void Draw(uint32_t count, uint32_t offset) override;

    std::shared_ptr<InputLayout> CreateInputLayout(const InputLayoutDesc& layout) override;
    std::shared_ptr<IndexBuffer> CreateIndexBuffer(const std::vector<uint32_t>& indices) override;

    std::shared_ptr<VertexBuffer> CreateVertexBuffer(const VertexData& vertices) override;
    std::shared_ptr<VertexBuffer> CreateVertexBuffer(const void* data, size_t size, size_t stride, UsageType usage) override;

    std::shared_ptr<ConstantBuffer> CreateConstantBuffer(const void* data, size_t size, UsageType usage) override;
    std::shared_ptr<ConstantBuffer> CreateConstantBuffer(std::shared_ptr<BufferData>& data) override;

    std::shared_ptr<PipelineState> CreatePipelineState(RasterState rs) override;

    std::shared_ptr<Sampler> CreateSampler(const SamplerBuilder& builder) override;
    std::shared_ptr<Sampler> CreateSampler(const SamplerState& ss) override;

    std::shared_ptr<Shader> CreateShader(ShaderType type, const TCHAR* file_name, const char* entry_point = nullptr,
        const char* target = nullptr, const std::vector<ShaderMacroEntry>& macros = {{nullptr, nullptr}}) override;

    std::shared_ptr<Program> CreateProgram(const char* name, const TCHAR* vs = nullptr, const TCHAR* ps = nullptr);

    std::shared_ptr<Texture> CreateTexture(const TextureBuilder& builder) override;
    std::shared_ptr<Query> CreateQuery(QueryType type, int capacity) override;

    std::shared_ptr<RenderTarget> CreateRenderTarget(uint32_t width, uint32_t height) override;

private:
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;

#if defined(_DEBUG)
    ComPtr<ID3D11Debug> d3d_debug_;
#endif

    std::unique_ptr<D3D11SwapChain> swap_chain_;
};

}
}
