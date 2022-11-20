#pragma once

#include <type_traits>
#include "Common/TypeTraits.h"
#include "lua.hpp"
#include "Checker.h"

namespace glacier {
namespace lux {

template<typename T>
void dispatch(lua_State* L, int index, int top, T& t) {
    if (index > top) {
        luaL_error(L, "function expected %d arguments at least", index);
    } else {
        Checker::Peek(L, index, t);
    }
}

template<typename T>
void dispatch(lua_State* L, int index, int top, optional<T>& t) {
    if (index <= top) {
        Checker::Peek(L, index, t);
    }
}

template<typename T, typename... Args>
void dispatch(lua_State* L, int index, int top, T& t, Args& ... args) {
    dispatch(L, index, top, t);
    dispatch(L, ++index, top, args...);
}

template<int offset = 0,typename T, typename... Args>
void peek(lua_State* L, T& t) {
    int top = lua_gettop(L);
    dispatch(L, 1 + offset, top, t);
}

template<int offset = 0, typename T, typename... Args>
void peek(lua_State* L, T& t, Args& ... args) {
    int top = lua_gettop(L);
    dispatch(L, 1 + offset, top, t, args...);
}

inline void push(lua_State* L, nullptr_t v) {
    lua_pushnil(L);
}

inline void push(lua_State* L) {}

template<typename T>
void push(lua_State* L, T&& t) {
    Checker::Push(L, std::forward<T>(t));
}

template<typename T, typename... Args>
void push(lua_State* L, T&& t, Args&&... args) {
    Checker::Push(L, std::forward<T>(t));
    push(L, std::forward<Args>(args)...);
}

template<typename... Args>
int ret(lua_State* L, Args&& ... args) {
    size_t size = sizeof...(args);
    push(L, std::forward<Args>(args)...);

    return (int)size;
}

template<typename T, 
    typename std::enable_if_t<std::is_enum<T>::value, int> = 0>
T get_arg(lua_State* L, int index) {
    int i;
    Checker::Peek(L, index, i);
    return (T)i;
}

template<typename T,
    typename std::enable_if_t<!std::is_enum<T>::value, int> = 0>
typename std::decay<T>::type get_arg(lua_State* L, int index) {
    typename std::decay<T>::type t;
    Checker::Peek(L, index, t);
    return t;
}

template<typename T>
void set_table(lua_State* L, int index, const char* key, T&& t) {
    lux::push(L, t);
    lua_setfield(L, index, key);
}

template<typename T, typename ...Args>
void set_table(lua_State* L, int index, const char* key, T&& t, Args&&... args) {
    lux::push(L, t);
    lua_setfield(L, index, key);
    set_table(L, index, std::forward<Args>(args)...);
}

}
}

#define LUX_MODULE_CONSTANT(L, libname, ...) do { \
    lua_newtable(L); \
    int idx = lua_gettop(L); \
    lux::set_table(L, idx, __VA_ARGS__); \
    lua_setglobal(L, libname); \
} while(0)

#define LUX_GLOBAL_CONSTANT(L, ...) do { \
    lua_pushglobaltable(L); \
    int idx = lua_gettop(L); \
    lux::set_table(L, idx, __VA_ARGS__); \
    lua_pop(L, 1); \
} while(0)
