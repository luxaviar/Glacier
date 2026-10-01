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

    uint32_t timestamp_capacity() const override;
    bool BeginTimestamp(CommandBuffer* cmd_buffer, uint32_t index) override;
    bool EndTimestamp(CommandBuffer* cmd_buffer, uint32_t index) override;
    bool ResolveTimestampRange(CommandBuffer* cmd_buffer, uint32_t frame_slot,
        uint32_t index, uint32_t count) override;
    bool GetTimestamp(uint32_t frame_slot, uint32_t index, double& seconds) override;

private:
    // the timestamps of the queue are in ticks of this frequency; it never
    // changes and reading it costs a call, so it is read once
    uint64_t TimestampFrequency();

    bool IsTimestampQuery() const { return query_type_ == D3D12_QUERY_TYPE_TIMESTAMP; }

    D3D12_QUERY_TYPE query_type_;
    size_t readback_entry_size_;
    ComPtr<ID3D12QueryHeap> heap_;
    ResourceLocation buffer_location_;
    // where the pairs of the API above land, kFrameSlots frames of them
    ResourceLocation pass_buffer_location_;
    uint64_t timestamp_frequency_ = 0;
};

}
}
