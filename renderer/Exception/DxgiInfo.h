#pragma once

#ifndef NDEBUG

#define DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#include <vector>
#include <string>
#include "Common/Singleton.h"

namespace glacier {

class DxgiInfo : public Singleton<DxgiInfo> {
public:
    void Set() noexcept;
    std::string GetMessages() const;

protected:
    DxgiInfo();

private:
    unsigned long long next_ = 0u;
    ComPtr<IDXGIInfoQueue> info_queue_;
};
}

#endif
