#pragma once

#include <d3d12.h>
#include <vector>
#include "Render/Base/Query.h"
#include "Buffer.h"

namespace glacier {
namespace render {

class D3D12GfxDriver;
class CommandBuffer;

class D3D12Query : public Query {
public:
    D3D12Query(D3D12GfxDriver* gfx, QueryType type, int capacity);

    void Begin(CommandBuffer* cmd_buffer) override;
    void End(CommandBuffer* cmd_buffer) override;
    QueryResult GetQueryResult(CommandBuffer* cmd_buffer) override;

private:
    D3D12_QUERY_TYPE query_type_;
    size_t readback_entry_size_;
    ComPtr<ID3D12QueryHeap> heap_;
    ResourceLocation buffer_location_;
};

}
}
