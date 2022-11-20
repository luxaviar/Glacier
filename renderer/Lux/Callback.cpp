#include "Callback.h"
#include "lstate.h"
#include "lua.hpp"
#include "Vm.h"

namespace glacier {

namespace lux {

int callback::prototype_counter = 0;

callback::callback(const callback& other) noexcept {
    ASSERT(other);

    prototype_ = other.prototype_;
    auto L = other.main_state_;
    ctx_ = (context*)lua_newuserdatauv(L, sizeof(context), other.ctx_->self ? 3 : 2); //L: ud
    ctx_->L = lua_newthread(L); //L: ud, th
    ctx_->self = other.ctx_->self;
    lua_setiuservalue(L, -2, 1); //L: ud

    lua_pushvalue(L, -1); //L: ud ud
    ref_ = luaL_ref(L, LUA_REGISTRYINDEX); // set ud ref; L: ud

    lua_rawgeti(L, LUA_REGISTRYINDEX, other.ref_); //L: ud, other ud

    lua_getiuservalue(L, -1, 2); //L: ud, other ud, fn
    lua_setiuservalue(L, -3, 2); //L: ud, other ud

    if (ctx_->self) {
        lua_getiuservalue(L, -1, 3); //L: ud, other ud, tb
        lua_setiuservalue(L, -3, 3); //L: ud, other ud
    }

    lua_pop(L, 1); //L: ud
    lua_pushcfunction(ctx_->L, LuaVM::ErrorHook); //l: hook

    lua_getiuservalue(L, -1, 2); //L: ud, fn
    lua_xmove(L, ctx_->L, 1); //L:ud, l: hook, fn

    if (ctx_->self) {
        lua_getiuservalue(L, -1, 3); //L: ud, tb
        lua_xmove(L, ctx_->L, 1); //L:ud, l: hook, fn, tb
    }
    lua_pop(L, 1); //L:

    main_state_ = L;
    base_top_ = ctx_->self ? 3 : 2;
}

callback::callback(const function& func) noexcept {
    prototype_ = ++prototype_counter;
    auto L = func.state;
    auto index = func.index;
    luaL_checktype(L, index, LUA_TFUNCTION);
    lua_pushvalue(L, index); //L: fn
    ctx_ = (context*)lua_newuserdatauv(L, sizeof(context), 2); //L: fn, ud
    ctx_->L = lua_newthread(L); //L: fn, ud, th
    ctx_->self = false;
    lua_setiuservalue(L, -2, 1); //L: fn, ud

    lua_pushvalue(L, -2); //L: fn, ud, fn
    lua_setiuservalue(L, -2, 2); //L: fn, ud

    ref_ = luaL_ref(L, LUA_REGISTRYINDEX); // set ud ref; L: fn

    lua_pushcfunction(ctx_->L, LuaVM::ErrorHook); //l: hook
    lua_xmove(L, ctx_->L, 1); //L: ; l: hook, fn
    main_state_ = L->l_G->mainthread;
    base_top_ = 2;
}

callback::callback(const function& func, const table& self) noexcept {
    prototype_ = ++prototype_counter;
    ASSERT(func.state == self.state);
    auto L = func.state;
    auto index = func.index;
    luaL_checktype(L, index, LUA_TFUNCTION);
    lua_pushvalue(L, index); //L: fn
    ctx_ = (context*)lua_newuserdatauv(L, sizeof(context), 3); //L: fn, ud
    ctx_->L = lua_newthread(L); //L: fn, ud, th
    ctx_->self = true;

    lua_setiuservalue(L, -2, 1); //L: fn, ud

    lua_pushvalue(L, -2); //L: fn, ud, fn
    lua_setiuservalue(L, -2, 2); //L: fn, ud

    lua_pushvalue(L, self.index); //L: fn, ud, tb
    lua_setiuservalue(L, -2, 3); //L: fn, ud

    ref_ = luaL_ref(L, LUA_REGISTRYINDEX); //  L: fn

    lua_pushcfunction(ctx_->L, LuaVM::ErrorHook); //l: hook
    lua_xmove(L, ctx_->L, 1); //L: ; l: hook, fn

    lua_pushvalue(self.state, self.index);
    lua_xmove(self.state, ctx_->L, 1); //l: hook, fn, tb

    main_state_ = L->l_G->mainthread;
    base_top_ = 3;
}

callback::~callback() {
    Dispose();
}

void callback::Dispose() {
    if (!ctx_) return;
    luaL_unref(main_state_, LUA_REGISTRYINDEX, ref_);

    ref_ = LUA_NOREF;
    main_state_ = nullptr;
    ctx_ = nullptr;
}


callback_stack::callback_stack(const function& func) noexcept :
    prototype_(func)
{
}

callback_stack::callback_stack(const function& func, const table& self) noexcept :
    prototype_(func, self)
{

}

callback* callback_stack::pop() {
    if (!stack_.empty()) {
        callback* cb = stack_.top();
        stack_.pop();
        return cb;
    }

    callback* cb = new callback(prototype_);
    return cb;
}

void callback_stack::push(callback* cb) {
    ASSERT(cb->prototype() == prototype_.prototype());

    // cb->Restore();
    stack_.push(cb);
}

}
}

