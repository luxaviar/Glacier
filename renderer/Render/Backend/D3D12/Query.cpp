#include "Query.h"
#include "Render/Backend/D3D12/GfxDriver.h"

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
    buffer_ = std::make_shared<D3D12ReadbackBuffer>(readback_entry_size_ * capacity);
}

void D3D12Query::Begin() {
    auto idx = (uint32_t)(++frame_ % capacity_);
    auto cmd_list = D3D12GfxDriver::Instance()->GetCommandList();

    if (query_type_ == D3D12_QUERY_TYPE_TIMESTAMP) {
        cmd_list->EndQuery(heap_.Get(), query_type_, idx * 2);
    }
    else {
        cmd_list->BeginQuery(heap_.Get(), query_type_, idx);
    }
}   

void D3D12Query::End() {
    auto idx = (uint32_t)(frame_ % capacity_);
    auto cmd_list = D3D12GfxDriver::Instance()->GetCommandList();

    if (query_type_ == D3D12_QUERY_TYPE_TIMESTAMP) {
        auto begin_idx = idx * 2;
        cmd_list->EndQuery(heap_.Get(), query_type_, begin_idx + 1);
        cmd_list->ResolveQueryData(heap_.Get(), query_type_, begin_idx, 2, buffer_->GetUnderlyingResource().Get(), idx * readback_entry_size_);
    }
    else {
        cmd_list->EndQuery(heap_.Get(), query_type_, idx);
        cmd_list->ResolveQueryData(heap_.Get(), query_type_, idx, 1, buffer_->GetUnderlyingResource().Get(), idx * readback_entry_size_);
    }
}

bool D3D12Query::QueryResultAvailable() {
    return true;
}

QueryResult D3D12Query::GetQueryResult() {
    auto cnt = frame_ - capacity_ + 1; //always get the last available
    if (cnt < 0) {
        return {};
    }

    QueryResult result = {};
    result.is_valid = true;

    auto idx = (uint32_t)(cnt % capacity_);
    auto cmd_list = D3D12GfxDriver::Instance()->GetCommandList();
    auto cmd_queue = D3D12GfxDriver::Instance()->GetCommandQueue()->GetUnderlyingCommandQueue();

    switch (type_)
    {
    case QueryType::kTimeStamp:
    {
        uint64_t freq;
        GfxThrowIfFailed(cmd_queue->GetTimestampFrequency(&freq));

        const uint64_t* query_data = buffer_->Map<uint64_t>();
        uint32_t begin_idx = idx * 2;
        uint32_t end_idx = begin_idx + 1;

        uint64_t begin_time = query_data[begin_idx];
        uint64_t end_time = query_data[end_idx];

        buffer_->Unmap();

        uint64_t delta = end_time - begin_time;
        result.elapsed_time = delta / (double)freq;
    }
        break;
    case QueryType::kOcclusion:
    {
        const auto query_data = buffer_->Map<uint64_t>();
        result.num_samples = query_data[idx];
        buffer_->Unmap();
    }
        break;
    case QueryType::kBinaryOcclusion:
    {
        const auto query_data = buffer_->Map<uint64_t>();
        result.any_samples = query_data[idx] == 1;
        buffer_->Unmap();
    }
        break;
    case QueryType::kPipelineStatistics:
    {
        const auto query_data = buffer_->Map<D3D12_QUERY_DATA_PIPELINE_STATISTICS>();
        auto& info = query_data[idx];
        result.primitives_rendered = info.IAPrimitives;
        result.vertices_rendered = info.IAVertices;
        buffer_->Unmap();
    }
        break;
    case QueryType::kStreamOutputStaticstics:
    {
        const auto query_data = buffer_->Map<D3D12_QUERY_DATA_SO_STATISTICS>();
        auto& info = query_data[idx];
        result.transform_feedback_primitives = info.NumPrimitivesWritten;
        buffer_->Unmap();
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
