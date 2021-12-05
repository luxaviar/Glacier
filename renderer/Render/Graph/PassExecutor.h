#pragma once
#include "render/base/gfxdriver.h"

namespace glacier {
namespace render {

class PassNode;
class Renderer;

class PassExecutor {
public:
    PassExecutor() {}
    virtual ~PassExecutor() {}
    PassExecutor(PassExecutor const&) = delete;
    PassExecutor& operator = (PassExecutor const&) = delete;

    virtual void Execute(Renderer* renderer, PassNode& pass) noexcept = 0;
};

template<typename Data, typename ExecuteFunc>
class RenderPassExecutor : public PassExecutor {
public:
    RenderPassExecutor(ExecuteFunc&& execute) : execute_(std::move(execute)) {}

    Data& data() { return data_; }
    const Data& data() const { return data_; }

    void Execute(Renderer* renderer, PassNode& pass) noexcept override {
        if constexpr (std::is_same<Data, nullptr_t>::value) {
            execute_(renderer, pass);
        }
        else {
            execute_(renderer, data_, pass);
        }
    }

private:
    Data data_;
    ExecuteFunc execute_;
};

}
}
