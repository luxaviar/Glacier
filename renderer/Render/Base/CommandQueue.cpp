#include "CommandQueue.h"
#include "Lux/Lux.h"

namespace glacier {
namespace render {

LUX_IMPL(CommandQueue, CommandQueue)
//LUX_CTOR(CommandQueue, GfxDriver*, CommandBufferType)
LUX_FUNC(CommandQueue, GetCommandBuffer)
LUX_FUNC(CommandQueue, LFlush)
LUX_FUNC(CommandQueue, LExecuteCommandBuffer)
LUX_IMPL_END

CommandQueue::CommandQueue(GfxDriver* driver, CommandBufferType type) :
    driver_(driver),
    type_(type)
{

}

CommandBuffer* CommandQueue::GetCommandBuffer() {
    CommandBuffer* command_list;

    if (!available_command_lists_.TryPop(command_list)) {
        auto pointer = CreateCommandBuffer();
        command_list = pointer.get();

        command_list_pool_.Push(std::move(pointer));
    }

    return command_list;
}

}
}
