#pragma once

namespace glacier {
namespace render {

class CommandBuffer;

// Whoever wants to be told when a pass of the render graph starts and ends. The
// GPU pass timer of PerfStats is what asks for it today: it writes a timestamp
// pair around every pass, which is how a frame's GPU time is split into the
// passes that make it up.
class PassObserver {
public:
    virtual ~PassObserver() {}

    virtual void BeginPass(const char* name, CommandBuffer* cmd_buffer) = 0;
    virtual void EndPass(CommandBuffer* cmd_buffer) = 0;
};

}
}
