#include "Fiber.h"

namespace glacier {
namespace jobs {

Fiber::Fiber(LPVOID address) : address_(address)
{
    
}

Fiber::~Fiber() {
    Release();
}

void Fiber::Init(const Delegate& callback, void* param) {
    assert(!address_);

    callback_ = callback;
    param_ = param;

    address_ = CreateFiber(0, &EntryPoint, this);
    state_ = ExecutionState::PENDING;
    ThrowIfLastExcept("CreateFiber(...)");
}

void Fiber::Init(Delegate&& callback, void* param) {
    assert(!address_);

    callback_ = std::move(callback);
    param_ = param;

    address_ = CreateFiber(0, &EntryPoint, this);
    state_ = ExecutionState::PENDING;
    ThrowIfLastExcept("CreateFiber(...)");
}

void Fiber::Init(LPVOID address) {
    assert(!address_);

    address_ = address;
    state_ = ExecutionState::PENDING;
    ThrowIfLastExcept("CreateFiber(...)");
}

void Fiber::Release() noexcept {
    if (address_ && callback_) { // release fibers by CreateFiber
        DeleteFiber(address_);
        address_ = NULL;
    }
}

void WINAPI Fiber::EntryPoint(LPVOID _In_ lpParameter) {
    Fiber* fiber = (Fiber*)lpParameter;
    fiber->callback_(fiber->param_);
}

}
}
