#include "Register.h"
#include <iostream>

namespace glacier {
namespace lux {

Register::PreloadLib::PreloadLib(const char* name_, lua_CFunction fn_) : name(name_), fn(fn_) {
    BinderFunction func = std::bind(&PreloadLib::Bind, this, std::placeholders::_1);
    Register::Instance()->AddPreloadLib(func);
}

void Register::PreloadLib::Bind(lua_State* L) {
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    lua_pushcfunction(L, fn);
    lua_setfield(L, -2, name.c_str());
    lua_pop(L, 1);
}

Register::Function::Function(const char* name_, lua_CFunction fn_) : name(name_), fn(fn_) {
    Register::Instance()->AddFunction(this);
}

Register* Register::self_ = nullptr;

void Register::AddFunction(Function* func) {
    auto it = functions_.find(func->name);
    if (it != functions_.end()) {
        std::cerr << "Duplicated Global Function Name: " << func->name;
        exit(-1);
    }

    functions_.emplace(func->name, func);
    BinderFunction fn = std::bind(&Register::BindFunction, this, func, std::placeholders::_1);
    binder_list_.push_back(fn);
}

void Register::BindFunction(Function* func, lua_State* L) {
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    lua_pushcfunction(L, LoadFunction);
    lua_setfield(L, -2, func->name.c_str());
    lua_pop(L, 1);
}

int Register::LoadFunction(lua_State* L) {
    const char* name = lua_tostring(L, 1);
    auto it = Instance()->functions_.find(name);
    if (it != Instance()->functions_.end()) {
        lua_pushcfunction(L, it->second->fn);
        return 1;
    }

    return 0;
}

void Register::AddConstant(ConstantLoader* loader) {
    constants_.emplace(loader->name, loader);
    BinderFunction fn = std::bind(&Register::BindConstant, this, loader, std::placeholders::_1);
    binder_list_.push_back(fn);
}

void Register::BindConstant(ConstantLoader* loader, lua_State* L) {
    luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
    lua_pushcfunction(L, LoadConstant);
    lua_setfield(L, -2, loader->name.c_str());
    lua_pop(L, 1);
}

int Register::LoadConstant(lua_State* L) {
    const char* name = lua_tostring(L, 1);
    auto it = Instance()->constants_.find(name);
    if (it != Instance()->constants_.end()) {
        return it->second->Load(L);
    }

    return 0;
}

void Register::AddPreloadLib(BinderFunction fn) {
    binder_list_.push_back(fn);
}

void Register::AddKlass(BinderFunction fn) {
    klass_list_.push_back(fn);
}

void Register::Bind(lua_State* L) {
    for(auto& fn : klass_list_) {
        fn(L);
    }

    for(auto& fn : binder_list_) {
        fn(L);
    }
}

}
}

