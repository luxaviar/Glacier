#include "Query.h"
#include "Render/Backend/D3D12/GfxDriver.h"
#include "CommandBuffer.h"

namespace glacier {
namespace render {

D3D12Query::D3D12Query(D3D12GfxDriver* gfx, QueryType type, int capacity) :
    Query(type, capacity)
{
    assert(capacity > 0);

    D3D12_QUERY_HEAP_DESC heap_desc = { };
    heap_desc.Count = capacity;
    heap_desc.NodeMask = 0;

    switch (type)
    {
    case QueryType::kTimeStamp:
        heap_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        query_type_ = D3D12_QUERY_TYPE_TIMESTAMP;
        heap_desc.Count = capacity * 2;
        readback_entry_size_ = sizeof(uint64_t) * 2;
        break;
    case QueryType::kOcclusion:
    case QueryType::kBinaryOcclusion:
        heap_desc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
        query_type_ = type == QueryType::kOcclusion ?  D3D12_QUERY_TYPE_OCCLUSION : D3D12_QUERY_TYPE_BINARY_OCCLUSION;
        readback_entry_size_ = sizeof(uint64_t);
        break;
    case QueryType::kPipelineStatistics:
        heap_desc.Type = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
        query_type_ = D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
        readback_entry_size_ = sizeof(D3D12_QUERY_DATA_PIPELINE_STATISTICS);
        break;
    case QueryType::kStreamOutputStaticstics:
        heap_desc.Type = D3D12_QUERY_HEAP_TYPE_SO_STATISTICS;
        query_type_ = D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0; //FIXME
        readback_entry_size_ = sizeof(D3D12_QUERY_DATA_SO_STATISTICS);
        break;
    default:
        break;
    }

    gfx->GetDevice()->CreateQueryHeap(&heap_desc, IID_PPV_ARGS(&heap_));
    auto readback_allocator = gfx->GetReadbackBufferAllocator();
    buffer_location_ = readback_allocator->CreateResource(readback_entry_size_ * capacity);

    // the pairs of the pass timer live in their own buffer: they are resolved a
    // whole frame at a time and read a frame later, which the ring of Begin/End
    // does not have room for
    if (query_type_ == D3D12_QUERY_TYPE_TIMESTAMP) {
        pass_buffer_location_ = readback_allocator->CreateResource(
            readback_entry_size_ * capacity * Query::kFrameSlots);
    }
}

uint64_t D3D12Query::TimestampFrequency() {
    if (timestamp_frequency_ == 0) {
        auto cmd_queue = D3D12GfxDriver::Instance()->GetCommandQueue()->GetNativeCommandQueue();
        GfxThrowIfFailed(cmd_queue->GetTimestampFrequency(&timestamp_frequency_));
    }

    return timestamp_frequency_;
}

uint32_t D3D12Query::timestamp_capacity() const {
    return IsTimestampQuery() ? (uint32_t)capacity_ : 0;
}

bool D3D12Query::BeginTimestamp(CommandBuffer* cmd_buffer, uint32_t index) {
    if (!IsTimestampQuery() || index >= (uint32_t)capacity_) {
        return false;
    }

    auto cmd_list = static_cast<D3D12CommandBuffer*>(cmd_buffer);
    cmd_list->EndQuery(heap_.Get(), query_type_, index * 2);

    return true;
}

bool D3D12Query::EndTimestamp(CommandBuffer* cmd_buffer, uint32_t index) {
    if (!IsTimestampQuery() || index >= (uint32_t)capacity_) {
        return false;
    }

    auto cmd_list = static_cast<D3D12CommandBuffer*>(cmd_buffer);
    cmd_list->EndQuery(heap_.Get(), query_type_, index * 2 + 1);

    return true;
}

bool D3D12Query::ResolveTimestampRange(CommandBuffer* cmd_buffer, uint32_t frame_slot,
    uint32_t index, uint32_t count)
{
    if (!IsTimestampQuery() || pass_buffer_location_.IsEmpty() || frame_slot >= kFrameSlots ||
        count == 0 || index + count > (uint32_t)capacity_) {
        return false;
    }

    auto cmd_list = static_cast<D3D12CommandBuffer*>(cmd_buffer);
    const size_t offset = (size_t)frame_slot * capacity_ * readback_entry_size_;

    cmd_list->ResolveQueryData(heap_.Get(), query_type_, index * 2, count * 2,
        pass_buffer_location_.GetResource().Get(), offset);

    return true;
}

bool D3D12Query::GetTimestamp(uint32_t frame_slot, uint32_t index, double& seconds) {
    if (!IsTimestampQuery() || pass_buffer_location_.IsEmpty() || frame_slot >= kFrameSlots ||
        index >= (uint32_t)capacity_) {
        return false;
    }

    const uint64_t frequency = TimestampFrequency();
    if (frequency == 0) {
        return false;
    }

    const uint64_t* query_data = pass_buffer_location_.Map<uint64_t>();
    const size_t pair = ((size_t)frame_slot * capacity_ + index) * 2;
    const uint64_t begin_time = query_data[pair];
    const uint64_t end_time = query_data[pair + 1];
    pass_buffer_location_.Unmap();

    seconds = end_time > begin_time ? (end_time - begin_time) / (double)frequency : 0.0;
    return true;
}

void D3D12Query::Begin(CommandBuffer* cmd_buffer) {
    auto idx = (uint32_t)(++frame_ % capacity_);
    auto cmd_list = static_cast<D3D12CommandBuffer*>(cmd_buffer);

    if (query_type_ == D3D12_QUERY_TYPE_TIMESTAMP) {
        cmd_list->EndQuery(heap_.Get(), query_type_, idx * 2);
    }
    else {
        cmd_list->BeginQuery(heap_.Get(), query_type_, idx);
    }
}

void D3D12Query::End(CommandBuffer* cmd_buffer) {
    auto idx = (uint32_t)(frame_ % capacity_);
    auto cmd_list = static_cast<D3D12CommandBuffer*>(cmd_buffer);

    if (query_type_ == D3D12_QUERY_TYPE_TIMESTAMP) {
        auto begin_idx = idx * 2;
        cmd_list->EndQuery(heap_.Get(), query_type_, begin_idx + 1);
        cmd_list->ResolveQueryData(heap_.Get(), query_type_, begin_idx, 2, buffer_location_.GetResource().Get(), idx * readback_entry_size_);
    }
    else {
        cmd_list->EndQuery(heap_.Get(), query_type_, idx);
        cmd_list->ResolveQueryData(heap_.Get(), query_type_, idx, 1, buffer_location_.GetResource().Get(), idx * readback_entry_size_);
    }
}

QueryResult D3D12Query::GetQueryResult(CommandBuffer* cmd_buffer) {
    auto cnt = frame_ - capacity_ + 1; //always get the last available
    if (cnt < 0) {
        return {};
    }

    auto idx = (uint32_t)(cnt % capacity_);
    // the results were written by the frame that resolved them, and reading them
    // before the GPU got there reads whatever the buffer held; the command buffer
    // knows the fence of its last submission, which is that frame or a later one
    const uint64_t fence = cmd_buffer != nullptr ? cmd_buffer->submitted_fence() : 0;
    if (fence != 0 && !D3D12GfxDriver::Instance()->GetCommandQueue()->IsFenceComplete(fence)) {
        return {};
    }

    QueryResult result = {};
    result.is_valid = true;

    switch (type_)
    {
    case QueryType::kTimeStamp:
    {
        const uint64_t freq = TimestampFrequency();

        const uint64_t* query_data = buffer_location_.Map<uint64_t>();
        uint32_t begin_idx = idx * 2;
        uint32_t end_idx = begin_idx + 1;

        uint64_t begin_time = query_data[begin_idx];
        uint64_t end_time = query_data[end_idx];

        buffer_location_.Unmap();

        uint64_t delta = end_time - begin_time;
        result.elapsed_time = delta / (double)freq;
    }
    break;
    case QueryType::kOcclusion:
    {
        const auto query_data = buffer_location_.Map<uint64_t>();
        result.num_samples = query_data[idx];
        buffer_location_.Unmap();
    }
    break;
    case QueryType::kBinaryOcclusion:
    {
        const auto query_data = buffer_location_.Map<uint64_t>();
        result.any_samples = query_data[idx] == 1;
        buffer_location_.Unmap();
    }
    break;
    case QueryType::kPipelineStatistics:
    {
        const auto query_data = buffer_location_.Map<D3D12_QUERY_DATA_PIPELINE_STATISTICS>();
        auto& info = query_data[idx];
        result.primitives_rendered = info.IAPrimitives;
        result.vertices_rendered = info.IAVertices;
        buffer_location_.Unmap();
    }
    break;
    case QueryType::kStreamOutputStaticstics:
    {
        const auto query_data = buffer_location_.Map<D3D12_QUERY_DATA_SO_STATISTICS>();
        auto& info = query_data[idx];
        result.transform_feedback_primitives = info.NumPrimitivesWritten;
        buffer_location_.Unmap();
    }
    break;
    default:
        result.is_valid = false;
        break;
    }

    return result;
}

}
}
