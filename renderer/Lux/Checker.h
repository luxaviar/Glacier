#pragma once

#include "lua.hpp"
#include <utility>
#include <string>
#include <string_view>
#include <ctype.h>
#include <type_traits>
#include <vector>
#include <memory>
#include "Math/Quat.h"
#include "Common/Color.h"
#include "Wrapper.h"
#include "Common/TypeTraits.h"
#include "Klass.h"

namespace glacier {

namespace physics {
    struct RayHitResult;
}

struct AABB;
struct Ray;
class Buf;
class LAr;

namespace lux {

enum class LightweightValue : uint8_t {
    kVector2 = 0,
    kVector3,
    kQuaternion,
    kColor,
    kAABB,
    kRay,
    kRayHitResult,
    kMax
};

struct Checker {
    static int lightweight_ctor[(int)LightweightValue::kMax];

    static int Register(lua_State* L);

    static void Peek(lua_State* L, int index, bool& v) {
        auto ty = lua_type(L, index);
        if (ty == LUA_TNIL || ty == LUA_TNONE) {
            v = false;
            return;
        }

        luaL_checktype(L, index, LUA_TBOOLEAN);
        v = lua_toboolean(L, index) == 1;
    }

    static void Peek(lua_State* L, int index, table& v) {
        luaL_checktype(L, index, LUA_TTABLE);
        v.index = index;
        v.nil = false;
        v.state = L;
    }

    static void Peek(lua_State* L, int index, function& v) {
        luaL_checktype(L, index, LUA_TFUNCTION);
        v.index = index;
        v.nil = false;
        v.state = L;
    }

    static void Peek(lua_State* L, int index, string& v) {
        v.str = luaL_checklstring(L, index, &(v.len));
        v.state = L;
    }

    static void Peek(lua_State* L, int index, userdata& v) {
        luaL_checktype(L, index, LUA_TUSERDATA);
        v.ptr = lua_touserdata(L, index);
    }

    static void Peek(lua_State* L, int index, pointer& v) {
        luaL_checktype(L, index, LUA_TLIGHTUSERDATA);
        v.ptr = lua_topointer(L, index);
    }

    static void Peek(lua_State* L, int index, const char* &v) {
        v = luaL_checklstring(L, index, nullptr);
    }

    static void Peek(lua_State* L, int index, const void* &v) {
        v = luaL_checklstring(L, index, nullptr);
    }

    static void Peek(lua_State* L, int index, HWND& v) {
        luaL_checktype(L, index, LUA_TLIGHTUSERDATA);
        v = (HWND)lua_topointer(L, index);
    }

    template<typename T,
        typename std::enable_if_t<std::is_integral<T>::value && !std::is_same<bool, T>::value, int> = 0>
    static void Peek(lua_State* L, int index, T& v) {
        v = (T)luaL_checkinteger(L, index);
    }

    template<typename T,
        typename std::enable_if_t<std::is_floating_point<T>::value, int> = 0>
    static void Peek(lua_State* L, int index, T& v) {
        v = (T)luaL_checknumber(L, index);
    }

    template<typename T,
        typename std::enable_if_t<std::is_enum<T>::value, int> = 0>
    static void Peek(lua_State* L, int index, T& v) {
        v = (T)luaL_checkinteger(L, index);
    }

    //template<typename T,
        //typename std::enable_if<std::is_same<LAr*, T>{}, int>::type = 0>
    //static void Peek(lua_State* L, int index, T& v) {
        //ImplPeek(L, index, v);
    //}

    static void Peek(lua_State* L, int index, std::string& v) {
        ImplPeek(L, index, v);
    }

    static void Peek(lua_State* L, int index, std::string_view& v) {
        ImplPeek(L, index, v);
    }

    static void Peek(lua_State* L, int index, Vector2& v) {
        ImplPeek(L, index, v);
    }

    static void Peek(lua_State* L, int index, Vector3& v) {
        ImplPeek(L, index, v);
    }

    static void Peek(lua_State* L, int index, Quaternion& v) {
        ImplPeek(L, index, v);
    }

    static void Peek(lua_State* L, int index, Color& v) {
        ImplPeek(L, index, v);
    }

    static void Peek(lua_State* L, int index, AABB& v) {
        ImplPeek(L, index, v);
    }

    static void Peek(lua_State* L, int index, Ray& v) {
        ImplPeek(L, index, v);
    }

    template<typename T>
    static void Peek(lua_State* L, int index, optional<T>& v) {
        int ty = lua_type(L, index);
        if (ty != LUA_TNIL) {
            Peek(L, index, v.value);
            v.nil = false;
            v.index = index;
            v.state = L;
        }
    }

    template<typename T>
    static void Peek(lua_State* L, int index, std::vector<T>& v) {
        luaL_checktype(L, index, LUA_TTABLE);
        int i = 1;
        lua_rawgeti(L, index, i);

        while(!lua_isnil(L, -1)) {
            int top = lua_gettop(L);
            T e;
            Peek(L, top, e);
            v.push_back(e);
            lua_pop(L, 1);

            lua_rawgeti(L, index, ++i);
        }

        lua_pop(L, 1);
    }

    template<typename T>
    static void Peek(lua_State* L, int index, handle<T>& v) {
        v.ptr = lux::Klass<T>::CheckHandle(L, index);
    }

    template<typename T>
    static void Peek(lua_State* L, int index, std::unique_ptr<T>& v) {
        auto handle = lux::Klass<T>::CheckHandle(L, index);
        if (handle->shared) {
            luaL_error(L, "Try to peek unique_ptr from a shared pointer!");
        }

        handle->gc = false;
        v.reset(handle->ptr.raw);
    }

    template<typename T>
    static void Peek(lua_State* L, int index, std::shared_ptr<T>& v) {
        auto handle = lux::Klass<T>::CheckHandle(L, index);
        if (!handle->shared) {
            luaL_error(L, "Try to peek shared_ptr from a raw pointer!");
        }
        v = handle->ptr.shared;
    }

    template<typename P,
        typename std::enable_if<std::is_pointer<P>{} && std::is_class<typename std::remove_pointer<P>::type>{}, int>::type = 0>
    static void Peek(lua_State* L, int index, P &v) {
        if (lua_isnoneornil(L, index)) {
            v = nullptr;
        } else {
            v = lux::Klass<typename std::decay<typename std::remove_pointer<P>::type>::type>::Check(L, index);
        }
    }

    static void Push(lua_State* L, const bool v) {
        lua_pushboolean(L, v ? 1 : 0);
    }

    static void Push(lua_State* L, nullptr_t v) {
        lua_pushnil(L);
    }

    static void Push(lua_State* L, const char* v) {
        lua_pushstring(L, v);
    }

    static void Push(lua_State* L, char* v) {
        lua_pushstring(L, v);
    }

    static void Push(lua_State* L, const std::string& v) {
        lua_pushlstring(L, v.data(), v.size());
    }

    static void Push(lua_State* L, const pointer& v) {
        lua_pushlightuserdata(L, (void*)v.ptr);
    }

    static void Push(lua_State* L, const HWND& v) {
        lua_pushlightuserdata(L, (void*)v);
    }

    static void Push(lua_State* L, const Vector2& v) {
        ImplPush(L, v);
    }

    static void Push(lua_State* L, const Vector3& v) {
        ImplPush(L, v);
    }

    static void Push(lua_State* L, const Quaternion& v) {
        ImplPush(L, v);
    }

    static void Push(lua_State* L, const Color& v) {
        ImplPush(L, v);
    }
    
    static void Push(lua_State* L, const AABB& v) {
        ImplPush(L, v);
    }

    static void Push(lua_State* L, const Ray& v) {
        ImplPush(L, v);
    }

    static void Push(lua_State* L, const physics::RayHitResult& v) {
        ImplPush(L, v);
    }

    template<typename T,
        typename std::enable_if_t<std::is_integral<T>::value && !std::is_same<bool, T>::value, int> = 0>
    static void Push(lua_State* L, const T v) {
        lua_pushinteger(L, v);
    }

    template<typename T,
        typename std::enable_if_t<std::is_floating_point<T>::value, int> = 0>
    static void Push(lua_State* L, const T v) {
        lua_pushnumber(L, v);
    }

    template<typename T,
        typename std::enable_if_t<std::is_enum<T>::value, int> = 0>
    static void Push(lua_State* L, T v) {
        lua_pushinteger(L, toUType(v));
    }

    template<typename T>
    static void Push(lua_State* L, const optional<T>& v) {
        if (v.nil) {
            lua_pushnil(L);
        }
        else {
            Push(L, v.value);
        }
    }

    template<typename T>
    static void Push(lua_State* L, const gcable<T>& v) {
        if (!v.ptr) {
            lua_pushnil(L);
        }
        else {
            lux::Klass<T>::Push(L, v.ptr, true);
        }
    }

    template<typename T>
    static void Push(lua_State* L, const std::vector<T>& v) {
        lua_newtable(L);
        int tab = lua_gettop(L);
        int i = 0;
        for(auto& t : v) {
            lua_pushinteger(L, ++i);
            Push(L, t);
            lua_settable(L, tab);
        }
    }

    template<typename P,
        typename std::enable_if_t<std::is_pointer<P>::value && std::is_class<typename std::remove_pointer<P>::type>::value, int> = 0>
    static void Push(lua_State* L, const P v) {
        lux::Klass<typename std::remove_pointer<P>::type>::Push(L, v);
    }

    template<typename T,
        typename std::enable_if_t<std::is_class<T>::value && !is_template<T>::value, int> = 0>
    static void Push(lua_State * L, const T& v) {
        lux::Klass<T>::Push(L, &v);
    }

    template<typename T>
    static void Push(lua_State* L, const std::shared_ptr<T>& ptr) {
        lux::Klass<T>::Push(L, ptr);
    }

private:
    static void ImplPeek(lua_State* L, int index, std::string& v);
    static void ImplPeek(lua_State* L, int index, std::string_view& v);
    static void ImplPeek(lua_State* L, int index, Vector2& v);
    static void ImplPeek(lua_State* L, int index, Vector3& v);
    static void ImplPeek(lua_State* L, int index, Quaternion& v);
    static void ImplPeek(lua_State* L, int index, Color& v);
    static void ImplPeek(lua_State* L, int index, AABB& v);
    static void ImplPeek(lua_State* L, int index, Ray& v);

    static void ImplPush(lua_State* L, const Vector2& v);
    static void ImplPush(lua_State* L, const Vector3& v);
    static void ImplPush(lua_State* L, const Quaternion& v);
    static void ImplPush(lua_State* L, const Color& v);
    static void ImplPush(lua_State* L, const AABB& v);
    static void ImplPush(lua_State* L, const Ray& v);
    static void ImplPush(lua_State* L, const physics::RayHitResult& v);
};

}
}
