#pragma once

#include "resource.h"

namespace glacier {
namespace render {

struct QueryResult
{
    union
    {
        double   elapsed_time;                   // Returns the elapsed time in seconds between Query::Begin and Query::End.
        uint64_t primitives_rendered;
        uint64_t transform_feedback_primitives;   // Valid for QueryType::CountTransformFeedbackPrimitives. Returns the number of primtives written to stream out or transform feedback buffers.
        uint64_t num_samples;                    // Returns the number of samples written by the fragment shader between Query::Begin and Query::End.
        bool     any_samples;                    // Returns true if any samples were written by the fragment shader between Query::Begin and Query::End.
    };
    uint64_t vertices_rendered;
    // Are the results of the query valid?
    // You should check this before using the value.
    bool is_valid = false;
};

class CommandBuffer;

class Query : public Resource {
public:
    Query(QueryType type, int capacity) : 
        type_(type),
        capacity_(capacity)
    {}

    virtual void Begin(CommandBuffer* cmd_buffer) = 0;
    virtual void End(CommandBuffer* cmd_buffer) = 0;
    virtual QueryResult GetQueryResult(CommandBuffer* cmd_buffer) = 0;

    // Timestamp pairs addressed by the caller rather than by the ring that
    // Begin/End walks. Whoever times many spans of one frame - the pass timer of
    // PerfStats - picks the pair by index, resolves the whole range once at the
    // end of the frame, and reads it back once the frame that recorded it is
    // known to have completed. The results of kFrameSlots frames are kept apart
    // so that reading one does not race the next frame writing into it.
    static constexpr uint32_t kFrameSlots = 3;

    // How many pairs this query holds; 0 for a query that is not a timestamp one,
    // which is what tells the caller the API below is not there
    virtual uint32_t timestamp_capacity() const { return 0; }
    // Writes the timestamp that opens a pair (the GPU counts from here)
    virtual bool BeginTimestamp(CommandBuffer* cmd_buffer, uint32_t index) { return false; }
    // Writes the timestamp that closes it
    virtual bool EndTimestamp(CommandBuffer* cmd_buffer, uint32_t index) { return false; }
    // Copies `count` pairs starting at `index` into the results of a frame slot,
    // in one call, after the last pair of the frame was recorded
    virtual bool ResolveTimestampRange(CommandBuffer* cmd_buffer, uint32_t frame_slot,
        uint32_t index, uint32_t count) { return false; }
    // Reads a resolved pair of a slot, in seconds. The caller is the one that
    // knows from the frame fence whether the GPU got there yet.
    virtual bool GetTimestamp(uint32_t frame_slot, uint32_t index, double& seconds) { return false; }

protected:
    QueryType type_;
    // How many queries will be used to prevent stalling the GPU.
    int64_t capacity_;
    int64_t frame_ = -1;
};

}
}
