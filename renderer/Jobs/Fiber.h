#pragma once

#include <functional>
#include <atomic>
#include "Exception/Exception.h"
#include "Common/Uncopyable.h"

namespace glacier {
namespace jobs {

enum class ExecutionState {
    INVALID,
    PENDING,
    SUCCEED,
    FAILED,
    FORCE_TERMINATED,
};

class Fiber : private Uncopyable {
public:
    using Delegate = std::function<void(void*)>;

    Fiber() {}
    Fiber(LPVOID address);
    ~Fiber();

    Fiber& operator=(Fiber&& other) {
        if (this != &other) {
            std::swap(state_, other.state_);
            std::swap(address_, other.address_);
            std::swap(callback_, other.callback_);
            std::swap(param_, other.param_);
        }

        return *this;
    }

    void Init(const Delegate& callback, void* param);
    void Init(Delegate&& callback, void* param);
    void Init(LPVOID address);

    void Release() noexcept;

    LPVOID GetAddress() const noexcept { return address_; }
    operator LPVOID() const { return address_; }

private:
    static void WINAPI EntryPoint(_In_ LPVOID lpParameter);

    ExecutionState state_ = ExecutionState::INVALID;
    LPVOID address_ = nullptr;
    
    Delegate callback_;
    void* param_ = nullptr;
};

}
}

