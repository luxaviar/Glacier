#pragma once

#include "lua.hpp"

namespace glacier {

struct LuxGlobal {
    static int LogInfo(lua_State* L);
    static int LogWarn(lua_State* L);
    static int LogError(lua_State* L);
};

}
