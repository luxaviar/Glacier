#pragma once

#include "Common/Uncopyable.h"

namespace glacier {
namespace render {

class Fence : private Uncopyable {
public:
    Fence() {}
    virtual ~Fence() = default;

    virtual void Reset() = 0;
    //Returns true if Fence is signaled
    virtual bool GetStatus() = 0;
    //Returns false if the wait times out
    virtual bool Wait() = 0;
};

}
}