#include "Bindings.h"
#include "Lux.h"
#include "lstate.h"
#include "Common/Log.h"

namespace glacier {

LUX_GLOBAL_FUNC(LogInfo, LuxGlobal::LogInfo)
LUX_GLOBAL_FUNC(LogWarn, LuxGlobal::LogWarn)
LUX_GLOBAL_FUNC(LogError, LuxGlobal::LogError)

int LuxGlobal::LogInfo(lua_State* L) {
    size_t len;
    const char* str = lua_tolstring(L, 1, &len);
    LOG_LOG("{}", str);
    return 0;
}

int LuxGlobal::LogWarn(lua_State* L) {
    size_t len;
    const char* str = lua_tolstring(L, 1, &len);
    LOG_WARN("{}", str);
    return 0;
}

int LuxGlobal::LogError(lua_State* L) {
    size_t len;
    const char* str = lua_tolstring(L, 1, &len);
    LOG_ERR("{}", str);
    return 0;
}

}