#include "Wrapper.h"
#include "lstate.h"
#include "lua.hpp"

namespace glacier {

namespace lux {

void refable::make_ref(lua_State* L, int index) {
    lua_pushvalue(L, index);
    ref = luaL_ref(L, LUA_REGISTRYINDEX);
    main_state = L->l_G->mainthread;
}

}
}

