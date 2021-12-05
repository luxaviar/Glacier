#pragma once

#include "resource.h"

namespace glacier {
namespace render {

struct QueryResult
{
    union
    {
        double   elapsed_time;                   // Valid for QueryType::Timer queries. Returns the elapsed time in seconds between Query::Begin and Query::End.
        uint64_t primitives_generated;           // Valid for QueryType::CountPrimitives. Returns the number of primitives generated between Query::Begin and Query::End.
        uint64_t primitives_rendered;
        uint64_t transform_feedback_primitives;   // Valid for QueryType::CountTransformFeedbackPrimitives. Returns the number of primtives written to stream out or transform feedback buffers.
        uint64_t num_samples;                    // Valid for QueryType::CountSamples. Returns the number of samples written by the fragment shader between Query::Begin and Query::End.
        bool     any_samples;                    // Valid for QueryType::CountSamplesPredicate. Returns true if any samples were written by the fragment shader between Query::Begin and Query::End.
    };
    uint64_t vertices_rendered;
    // Are the results of the query valid?
    // You should check this before using the value.
    bool is_valid = false;
};

class Query : public Resource {
public:
    Query(QueryType type, int capacity) : 
        type_(type),
        capacity_(capacity)
    {}

    virtual void Begin() = 0;
    virtual void End() = 0;
    virtual bool QueryResultAvailable() = 0;
    virtual QueryResult GetQueryResult() = 0;

protected:
    QueryType type_;
    // How many queries will be used to prevent stalling the GPU.
    int64_t capacity_;
    int64_t frame_ = 0;
};

}
}
