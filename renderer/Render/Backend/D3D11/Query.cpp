#include "query.h"
#include "render/backend/d3d11/gfxdriver.h"

namespace glacier {
namespace render {

D3D11QueryElement::D3D11QueryElement(D3D11GfxDriver* gfx, QueryType type) :  type_(type) {
    D3D11_QUERY_DESC query_desc = {};
    switch (type)
    {
    case QueryType::kTimeStamp:
        query_desc.Query = D3D11_QUERY_TIMESTAMP;
        break;
    case QueryType::kOcclusion:
        query_desc.Query = D3D11_QUERY_OCCLUSION;
        break;
    case QueryType::kBinaryOcclusion:
        query_desc.Query = D3D11_QUERY_OCCLUSION_PREDICATE;
        break;
    case QueryType::kPipelineStatistics:
        query_desc.Query = D3D11_QUERY_PIPELINE_STATISTICS;
        break;
    default:
        query_desc.Query = D3D11_QUERY_SO_STATISTICS;
        throw std::exception{ "Bad QueryType to D3D11QueryElement." };
        break;
    }
    
    auto device = gfx->GetDevice();
    device->CreateQuery(&query_desc, &query_);

    // For timer queries, we also need to create the disjoint timer queries.
    if (type == QueryType::kTimeStamp) {
        D3D11_QUERY_DESC disjoint_query_desc = {};
        disjoint_query_desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;

        device->CreateQuery(&query_desc, &query_end_);
        device->CreateQuery(&disjoint_query_desc, &disjoint_query_);
    }
}

void D3D11QueryElement::Begin() {
    auto context = D3D11GfxDriver::Instance()->GetContext();
    if (type_ == QueryType::kTimeStamp) {
        context->Begin(disjoint_query_.Get());
        context->End(query_.Get());
    } else {
        context->Begin(query_.Get());
    }
}

void D3D11QueryElement::End() {
    auto context = D3D11GfxDriver::Instance()->GetContext();
    if (type_ == QueryType::kTimeStamp) {
        context->End(query_end_.Get());
        context->End(disjoint_query_.Get());
    } else {
        context->End(query_.Get());
    }
}

bool D3D11QueryElement::QueryResultAvailable() {
    auto context = D3D11GfxDriver::Instance()->GetContext();
    HRESULT result;
    if (type_ == QueryType::kTimeStamp) {
        result = context->GetData(disjoint_query_.Get(), nullptr, 0, 0);
    } else {
        result = context->GetData(query_.Get(), nullptr, 0, 0);
    }

    return result == S_OK;
}

QueryResult D3D11QueryElement::GetQueryResult() {
    QueryResult result = {};
    result.is_valid = true;
    auto context = D3D11GfxDriver::Instance()->GetContext();

    if (type_ == QueryType::kTimeStamp) { //Vsync have significant impact about GPU time
        while (context->GetData(disjoint_query_.Get(), nullptr, 0, 0) == S_FALSE) {
            Sleep(1L);
        }

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timeStampDisjoint;
        context->GetData(disjoint_query_.Get(), &timeStampDisjoint, sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0);
        if (timeStampDisjoint.Disjoint == FALSE) {
            UINT64 beginTime, endTime;
            if (context->GetData(query_.Get(), &beginTime, sizeof(UINT64), 0) == S_OK &&
                context->GetData(query_end_.Get(), &endTime, sizeof(UINT64), 0) == S_OK)
            {
                result.elapsed_time = (endTime - beginTime) / double(timeStampDisjoint.Frequency);
            }
        }
    } else {
        // Wait for the results to become available.
        while (context->GetData(query_.Get(), nullptr, 0, 0)) {
            Sleep(1L);
        }

        switch (type_)
        {
        case QueryType::kOcclusion:
        {
            UINT64 numSamples = 0;
            if (context->GetData(query_.Get(), &numSamples, sizeof(UINT64), 0) == S_OK)
            {
                result.num_samples = numSamples;
            }
        }
        break;
        case QueryType::kBinaryOcclusion:
        {
            BOOL anySamples = FALSE;
            if (context->GetData(query_.Get(), &anySamples, sizeof(UINT64), 0) == S_OK)
            {
                result.any_samples = anySamples == TRUE;
            }
        }
        break;
        case QueryType::kPipelineStatistics:
        {
            D3D11_QUERY_DATA_PIPELINE_STATISTICS pipeline_stats = {};
            if (context->GetData(query_.Get(), &pipeline_stats, sizeof(D3D11_QUERY_DATA_PIPELINE_STATISTICS), 0) == S_OK)
            {
                result.primitives_rendered = pipeline_stats.IAPrimitives;
                result.vertices_rendered = pipeline_stats.IAVertices;
            }
        }
        break;
        case QueryType::kStreamOutputStaticstics:
        {
            D3D11_QUERY_DATA_SO_STATISTICS streamOutStats = {};
            if (context->GetData(query_.Get(), &streamOutStats, sizeof(D3D11_QUERY_DATA_SO_STATISTICS), 0) == S_OK)
            {
                result.transform_feedback_primitives = streamOutStats.NumPrimitivesWritten;
            }
        }
        break;
        default:
            result.is_valid = false;
            break;
        }
    }

    return result;
}

D3D11Query::D3D11Query(D3D11GfxDriver* gfx, QueryType type, int capacity) : Query(type, capacity) {
    assert(capacity > 0);

    for (size_t i = 0; i < capacity; ++i) {
        queue_.emplace_back(gfx, type);
    }
}

void D3D11Query::Begin() {
    auto idx = ++frame_ % capacity_;
    queue_[idx].Begin();
}

void D3D11Query::End() {
    auto idx = frame_ % capacity_;
    queue_[idx].End();
}

bool D3D11Query::QueryResultAvailable() {
    auto idx = frame_ - capacity_ + 1; //always get the last available
    if (idx >= 0) {
        idx %= capacity_;
        return queue_[idx].QueryResultAvailable();
    } else {
        return false;
    }
}

QueryResult D3D11Query::GetQueryResult() {
    auto idx = frame_ - capacity_ + 1; //always get the last available
    if (idx >= 0) {
        idx %= capacity_;
        return queue_[idx].GetQueryResult();
    } else {
        return {};
    }
}

}
}
