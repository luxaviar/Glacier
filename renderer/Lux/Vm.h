#pragma once

#include <assert.h>
#include <map>
#include <vector>
#include <signal.h>
#include <optional>
#include "lua.hpp"
#include "Common/log.h"
#include "Common/Uncopyable.h"
#include "Lux/Lux.h"
#include "Lux/Callback.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace glacier {

class ScriptTimeCheckThread;
class Timer;
class ScriptDebug;

enum class LuaRef : int32_t {
    REF_IDLE = 0,
    REF_PASS_SECOND,
    REF_RESUME_CO,
    REF_APP_LAUNCH,
    REF_APP_CLOSE,
    REF_MAX
};

struct LuaStackKeeper : private Uncopyable {
    explicit LuaStackKeeper(lua_State* L_) {
        L = L_;
        top = lua_gettop(L);
    }
    ~LuaStackKeeper() {
        lua_settop(L, top);
    }

    lua_State* L;
    int top;
};

class LuaVM : private Uncopyable {
public:
    static int ErrorHook(lua_State* L);

    LuaVM();
    virtual ~LuaVM();

    lua_State* L() {
        assert(L_);
        return L_;
    }

    void Init(const char* args, const char* preload_script, const char* main_script);

    void Reload();
    void set_gcstep(int step) { gcstep_ = step; }

    void SetRef(int idx, lux::function fn);
    lux::callback_stack& GetRef(LuaRef ref);

    int mem() const;
    int mem_max() const { return mem_max_; }

    bool PCall(int nargs, int nrets);

    void Update(uint32_t dt_us);

    template<int ret = 0, typename... Args>
    bool CallRef(lua_Integer ref, Args&& ... args) {
        if (ref == LUA_NOREF || ref == LUA_REFNIL) {
            return false;
        }

        lua_rawgeti(L_, LUA_REGISTRYINDEX, ref);
        if (!lua_isfunction(L_, -1)) {
            lua_pop(L_, 1);
            LOG_ERR("Can't find ref {} to CallRef", (int64_t)ref);
            return false;
        }

        size_t size = sizeof...(args);
        lux::push(L_, std::forward<Args>(args)...);

        return PCall(size, ret);
    }

    template<int ret = 0, typename... Args>
    std::optional<const lux::callback*> Call(LuaRef ref, Args&& ... args) {
        auto& cb = GetRef(ref);
        if (!cb) {
            LOG_ERR("Can't find LuaRef {}", (int)ref);
            return {};
        }

        return cb.Invoke<ret>(std::forward<Args>(args)...);
    }

private:
    bool ScriptInit();
    bool DoFile(const char* file, int ret = 0);

    std::string args_;
    std::string preload_script_;
    std::string main_script_;

    lua_State* L_;
    int err_hook_pos_;
    int gcstep_;

    int mem_max_;
    uint32_t elapse_time_;
    uint32_t gc_time_;
    uint64_t stack_err_time_;

    lux::callback_stack ref_func_[toUType(LuaRef::REF_MAX)];
    std::map<std::string, std::string> map_conf_;

    ScriptTimeCheckThread* script_ctt_;
};

}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
