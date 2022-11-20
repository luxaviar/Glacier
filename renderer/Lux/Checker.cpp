#include <string.h>
#include <assert.h>
#include "Checker.h"
#include "Lux.h"
#include "Geometry/Aabb.h"
#include "Physics/Collider/Collider.h"
#include "Physics/Collision/Hitresult.h"

namespace glacier {

namespace lux {

LUX_CONSTANT_MULTI(LightweightValue, LightweightValue)
LUX_CONST("kVector2", LightweightValue::kVector2)
LUX_CONST("kVector3", LightweightValue::kVector3)
LUX_CONST("kQuaternion", LightweightValue::kQuaternion)
LUX_CONST("kColor", LightweightValue::kColor)
LUX_CONST("kRay", LightweightValue::kRay)
LUX_CONST("kAABB", LightweightValue::kAABB)
LUX_CONST("kRayHitResult", LightweightValue::kRayHitResult)
LUX_CONSTANT_MULTI_END

LUX_IMPL(Checker, Checker)
LUX_CTOR(Checker)
LUX_FUNC(Checker, Register)
LUX_IMPL_END

int Checker::lightweight_ctor[(int)LightweightValue::kMax] = {0};

int Checker::Register(lua_State* L) {
    int idx = luaL_checkinteger(L, 1);
    if (idx < 0 || idx > (int)LightweightValue::kMax) {
        luaL_error(L, "LightweightValue index out of bound");
    }

    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);
    lightweight_ctor[idx] = luaL_ref(L, LUA_REGISTRYINDEX);
    return 0;
}

void Checker::ImplPeek(lua_State* L, int index, std::string& v) {
    size_t size;
    const char* str = luaL_checklstring(L, index, &size);
    v.assign(str, size);
}

void Checker::ImplPeek(lua_State* L, int index, std::string_view& v) {
    size_t size;
    const char* str = luaL_checklstring(L, index, &size);
    std::string_view s(str, size);
    v.swap(s);
}

void Checker::ImplPeek(lua_State* L, int index, Vector2& v) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua_rawgeti(L, index, 1);
    float x = (float)luaL_checknumber(L, -1);
    lua_rawgeti(L, index, 2);
    float y = (float)luaL_checknumber(L, -1);
    v.Set(x, y);
    lua_pop(L, 2);
}

void Checker::ImplPeek(lua_State* L, int index, Vector3& v) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua_rawgeti(L, index, 1);
    float x = (float)luaL_checknumber(L, -1);
    lua_rawgeti(L, index, 2);
    float y = (float)luaL_checknumber(L, -1);
    lua_rawgeti(L, index, 3);
    float z = (float)luaL_checknumber(L, -1);
    v.Set(x, y, z);
    lua_pop(L, 3);
}

void Checker::ImplPeek(lua_State* L, int index, Quaternion& v) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua_rawgeti(L, index, 1);
    float x = (float)luaL_checknumber(L, -1);
    lua_rawgeti(L, index, 2);
    float y = (float)luaL_checknumber(L, -1);
    lua_rawgeti(L, index, 3);
    float z = (float)luaL_checknumber(L, -1);
    lua_rawgeti(L, index, 4);
    float w = (float)luaL_checknumber(L, -1);
    v.Set(x, y, z, w);
    lua_pop(L, 4);
}

void Checker::ImplPeek(lua_State* L, int index, Color& v) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua_rawgeti(L, index, 1);
    float x = (float)luaL_checknumber(L, -1);
    lua_rawgeti(L, index, 2);
    float y = (float)luaL_checknumber(L, -1);
    lua_rawgeti(L, index, 3);
    float z = (float)luaL_checknumber(L, -1);
    lua_rawgeti(L, index, 4);
    float w = (float)luaL_checknumber(L, -1);
    v.Set(x, y, z, w);
    lua_pop(L, 4);
}

void Checker::ImplPeek(lua_State* L, int index, AABB& v) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua_pushstring(L, "min");
    lua_rawget(L, index);
    int top = lua_gettop(L);

    ImplPeek(L, top, v.min);

    lua_pushstring(L, "max");
    lua_rawget(L, index);
    top = lua_gettop(L);
    ImplPeek(L, top, v.max);
    lua_pop(L, 2);
}

void Checker::ImplPeek(lua_State* L, int index, Ray& v) {
    luaL_checktype(L, index, LUA_TTABLE);

    lua_pushstring(L, "origin");
    lua_rawget(L, index);
    int top = lua_gettop(L);
    ImplPeek(L, top, v.origin);

    lua_pushstring(L, "direction");
    lua_rawget(L, index);
    top = lua_gettop(L);
    ImplPeek(L, top, v.direction);

    lua_pop(L, 2);
}

void Checker::ImplPush(lua_State* L, const Vector2& v) {
    int idx = lightweight_ctor[(int)LightweightValue::kVector2];
    if (idx == 0) {
        luaL_error(L, "ctor for Vector2 not found!");
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, idx);
    luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    lua_call(L, 2, 1);
}

void Checker::ImplPush(lua_State* L, const Vector3& v) {
    int idx = lightweight_ctor[(int)LightweightValue::kVector3];
    if (idx == 0) {
        luaL_error(L, "ctor for Vector3 not found!");
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, idx);
    luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    lua_pushnumber(L, v.z);
    lua_call(L, 3, 1);
}

void Checker::ImplPush(lua_State* L, const Quaternion& v) {
    int idx = lightweight_ctor[(int)LightweightValue::kQuaternion];
    if (idx == 0) {
        luaL_error(L, "ctor for Vector3 not found!");
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, idx);
    luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    lua_pushnumber(L, v.z);
    lua_pushnumber(L, v.w);
    lua_call(L, 4, 1);
}

void Checker::ImplPush(lua_State* L, const Color& v) {
    int idx = lightweight_ctor[(int)LightweightValue::kColor];
    if (idx == 0) {
        luaL_error(L, "ctor for Vector3 not found!");
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, idx);
    luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    lua_pushnumber(L, v.z);
    lua_pushnumber(L, v.w);
    lua_call(L, 4, 1);
}

void Checker::ImplPush(lua_State* L, const AABB& v) {
    int idx = lightweight_ctor[(int)LightweightValue::kAABB];
    if (idx == 0) {
        luaL_error(L, "ctor for AABB not found!");
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, idx);
    luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_pushnumber(L, v.min.x);
    lua_pushnumber(L, v.min.y);
    lua_pushnumber(L, v.min.z);
    lua_pushnumber(L, v.max.x);
    lua_pushnumber(L, v.max.y);
    lua_pushnumber(L, v.max.z);
    lua_call(L, 6, 1);
}

void Checker::ImplPush(lua_State* L, const Ray& v) {
    int idx = lightweight_ctor[(int)LightweightValue::kRay];
    if (idx == 0) {
        luaL_error(L, "ctor for Ray not found!");
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, idx);
    luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_pushnumber(L, v.origin.x);
    lua_pushnumber(L, v.origin.y);
    lua_pushnumber(L, v.origin.z);
    lua_pushnumber(L, v.direction.x);
    lua_pushnumber(L, v.direction.y);
    lua_pushnumber(L, v.direction.z);
    lua_call(L, 6, 1);
}

void Checker::ImplPush(lua_State* L, const physics::RayHitResult& v) {
    int idx = lightweight_ctor[(int)LightweightValue::kRayHitResult];
    if (idx == 0) {
        luaL_error(L, "ctor for RayHitResult not found!");
    }
    lua_rawgeti(L, LUA_REGISTRYINDEX, idx);
    luaL_checktype(L, -1, LUA_TFUNCTION);
    lua_pushboolean(L, v.hit);
    lua_pushnumber(L, v.t);
    if (v.collider) {
        lux::Klass<glacier::Collider>::Push(L, v.collider);
    } else {
        lua_pushnil(L);
    }
    lua_call(L, 3, 1);
}

}
}

