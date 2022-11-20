#pragma once

#include <optional>
#include <stack>
#include "Stack.h"
#include "Common/Log.h"
#include "Common/Uncopyable.h"

namespace glacier {
namespace lux {

class callback : private Uncopyable {
public:
    struct context {
        lua_State* L;
        bool self;
    };

    callback() noexcept {}
    callback(const callback& other) noexcept;
    callback(const function& func) noexcept;
    callback(const function& func, const table& self) noexcept;
    callback(callback&& other) noexcept {
        *this = std::move(other);
    }
    ~callback();

    callback& operator=(callback&& other) noexcept {
        std::swap(ref_, other.ref_);
        std::swap(main_state_, other.main_state_);
        std::swap(base_top_, other.base_top_);
        std::swap(ctx_, other.ctx_);
        std::swap(other.prototype_, prototype_);
        return *this;
    }

    operator bool() const {
        return ctx_ != nullptr;
    }

    int prototype() const { return prototype_; }
    lua_State* L() const { return ctx_ ? ctx_->L : nullptr; }

    void Dispose();

    void Restore() const {
        lua_settop(ctx_->L, base_top_);
    }

    void SetupCall() const {
        auto L = ctx_->L;
        auto top = lua_gettop(L);
        if (top != base_top_) {
            lua_settop(L, base_top_);
            LOG_ERR("Invalid stack number when setup lua callback: {}", top);
        }

        lua_pushvalue(L, 2);
        if (ctx_->self) {
            lua_pushvalue(L, 3);
        }
    }

    template<int ret=0, typename... Args>
    std::optional<const callback*> Invoke(Args&& ...args) const {
        ASSERT(ctx_);
        SetupCall();

        size_t nargs = sizeof...(args);
        lux::push(ctx_->L, std::forward<Args>(args)...);

        return Call((int)nargs, ret);
    }

    std::optional<const callback*> Call(int nargs, int nrets) const {
        ASSERT(ctx_);

        constexpr int err_hook = 1;
        auto L = ctx_->L;
        auto top = lua_gettop(L);
        
        nargs += (ctx_->self ? 1 : 0);
        if (top != base_top_ + nargs + 1) {
            LOG_ERR("Invalid stack number when call lua callback: {}, args: ", top, nargs);
            return {};
        }

        int r = lua_pcall(L, nargs, nrets, err_hook);
        if (r != LUA_OK) {
            lua_pop(L, 1);
            return {};
        }

        return { this };
    }

private:
    static int prototype_counter;
    int prototype_ = 0;
    int ref_ = LUA_NOREF;
    int base_top_ = 0;
    lua_State* main_state_ = nullptr;
    context* ctx_ = nullptr;
};

class callback_stack : private Uncopyable {
public:
    struct guard {
        callback_stack* stack_;
        callback* callback_;
        guard(callback_stack* stack, callback* callback) noexcept :
            stack_(stack),
            callback_(callback)
        {

        }
        ~guard() {
            stack_->push(callback_);
        }
    };

    callback_stack() noexcept {}
    callback_stack(const function& func) noexcept;
    callback_stack(const function& func, const table& self) noexcept;

    callback_stack& operator=(callback_stack&& other) noexcept {
        std::swap(other.prototype_, prototype_);
        std::swap(other.stack_, stack_);
        return *this;
    }
    
    operator bool() const {
        return (bool)prototype_;
    }

    callback* pop();
    void push(callback* cb);

    template<int ret=0, typename... Args>
    std::optional<const callback*> Invoke(Args&& ...args) {
        auto callback = pop();
        guard guard(this, callback);
        auto result = callback->Invoke<ret>(std::forward<Args>(args)...);
        return result;
    }

private:
    callback prototype_;
    std::stack<callback*> stack_;
};

}
}
