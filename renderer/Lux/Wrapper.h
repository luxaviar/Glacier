#pragma once

#include <memory>
#include "lua.hpp"
#include "Klass.h"
#include "Common/Uncopyable.h"

namespace glacier {
namespace lux {

struct pointer {
    const void* ptr = nullptr;

    operator const void* () { return ptr; }
    operator void* () { return (void*)ptr; }
    operator bool() { return ptr != nullptr; }
};

struct userdata {
    void* ptr = nullptr;

    operator void* () { return ptr; }
    operator bool() { return ptr != nullptr; }
};

struct string {
    const char* str;
    size_t len;
    lua_State* state;

    string() noexcept : str(nullptr), len(0), state(nullptr) {
    }

    string(const char* s) : state(nullptr) {
        str = s;
        len = strlen(s);
    }

    operator const char* () {
        return str;
    }

    operator char* () {
        return (char*)str;
    }

    operator unsigned char* () {
        return (unsigned char*)str;
    }

    operator const unsigned char* () {
        return (const unsigned char*)str;
    }

    operator void* () {
        return (void*)str;
    }
};

struct table {
    bool nil;
    int index;
    lua_State* state;

    table() noexcept : nil(true), index(0), state(nullptr) {}
    table(lua_State* L, int index) noexcept : nil(false), index(index), state(L) {}
    table(const table& other) noexcept = default;

    operator int() noexcept {
        return index;
    }
};

struct function {
    bool nil;
    int index;
    lua_State* state;

    function() noexcept : nil(true), index(0), state(nullptr) {}
    function(lua_State* L, int index) noexcept : nil(false), index(index), state(L) {}
    function(const function& other) noexcept = default;

    operator int() noexcept {
        return index;
    }
};

class refable : private Uncopyable {
public:
    refable() noexcept : ref(LUA_NOREF), main_state(nullptr) {}

    explicit refable(const function& func) {
        make_ref(func.state, func.index);
    }

    explicit refable(const table& tab) {
        make_ref(tab.state, tab.index);
    }

    refable(refable&& other) noexcept : refable() {
        swap(other);
    }

    virtual ~refable() {
        unref();
    }

    refable& operator=(refable&& other) noexcept {
        swap(other);
        return *this;
    }

    refable& operator=(const function& func) {
        unref();
        make_ref(func.state, func.index);
        return *this;
    }

    refable& operator=(const table& tab) {
        unref();
        make_ref(tab.state, tab.index);
        return *this;
    }

    operator lua_Integer() const noexcept {
        return ref;
    }

    operator bool() const noexcept {
        return valid();
    }

    void swap(refable& other) noexcept {
        std::swap(ref, other.ref);
        std::swap(main_state, other.main_state);
    }

    bool valid() const noexcept {
        return ref != LUA_NOREF && ref != LUA_REFNIL;
    }

    void unref() {
        if (valid() && main_state) {
            luaL_unref(main_state, LUA_REGISTRYINDEX, ref);
            ref = LUA_NOREF;
        }
    }

    bool push_ref(lua_State* L) {
        if (valid()) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            return true;
        }
        else {
            lua_pushnil(L);
        }
        return false;
    }

private:
    void make_ref(lua_State* L, int index);

    int ref;
    lua_State* main_state;
};

template<typename T>
struct optional {
    optional() : nil(true) {}
    optional(const T& v) : value(v) {}
    optional(T&& v) : value(std::move(v)) {}

    optional(const optional<T>&) = default;
    optional(optional<T>&&) = default;

    optional<T>& operator=(T v) { value = v; return *this; }

    operator T&() { return value; }

    bool nil = true;
    int index;
    lua_State* state;
    T value;
};

template<typename T>
struct gcable {
    gcable(T* p) : ptr(p) {}
    gcable(std::unique_ptr<T>& p) : ptr(p.release()) {}
    T* ptr;
};

template<typename T>
struct handle {
    handle(typename Klass<T>::Handle* p) : ptr(p) {}

    void ungcable() {
        ptr->gc = false;
    }

    typename Klass<T>::Handle* ptr;
};

}
}
