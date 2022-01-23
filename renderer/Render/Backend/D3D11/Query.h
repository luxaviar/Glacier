#pragma once

#include <d3d11_1.h>
#include <vector>
#include "render/base/query.h"

namespace glacier {
namespace render {

class D3D11GfxDriver;

class D3D11QueryElement {
public:
    D3D11QueryElement(D3D11GfxDriver* gfx, QueryType type);

    void Begin();
    void End();
    bool QueryResultAvailable();
    QueryResult GetQueryResult();
    
private:
    ComPtr<ID3D11Query> query_;

    // For timer queries, we need 2 sets of buffered queries && disjoint query.
    ComPtr<ID3D11Query> disjoint_query_;
    ComPtr<ID3D11Query> query_end_;

    QueryType type_;
};

class D3D11Query : public Query {
public:
    D3D11Query(D3D11GfxDriver* gfx, QueryType type, int capacity);

    void Begin() override;
    void End() override;
    bool QueryResultAvailable() override;
    QueryResult GetQueryResult() override;

private:
    std::vector<D3D11QueryElement> queue_;
};

}
}
