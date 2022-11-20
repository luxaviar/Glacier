#include "Vm.h"
#include <stdio.h>
#include <sstream>
#include "lstate.h"
#include "Monitor.h"
#include "Common/ByteStream.h"
#include "Inspect/Profiler.h"

namespace glacier {

LUX_CONSTANT_MULTI(SERVICE_REF, LuaRef)
LUX_CONST("PASS_SECOND", LuaRef::REF_PASS_SECOND)
LUX_CONST("RESUME_CO", LuaRef::REF_RESUME_CO)
LUX_CONST("APP_LAUNCH", LuaRef::REF_APP_LAUNCH)
LUX_CONST("APP_CLOSE", LuaRef::REF_APP_CLOSE)
LUX_CONSTANT_MULTI_END

int LuaVM::ErrorHook(lua_State* L) {
    const char* errmsg = lua_tostring(L, -1);

    std::ostringstream errdump(4096);
    errdump << "[LUA] ";
    errdump.write(errmsg, strlen(errmsg));
    errdump << "\nstack traceback:";

    lua_Debug ldb;
    for (int i = 0; lua_getstack(L, i, &ldb) == 1; ++i) {
        errdump << "\n\t";
        lua_getinfo(L, "Slnu", &ldb);
        const char* name = ldb.name ? ldb.name : "";
        const char* filename = ldb.source ? ldb.source : "";
        errdump << ldb.what << " '" << name << "' @'" << filename << ":" << ldb.currentline << "'";

        bool local_out = false;
        const char* var_name = nullptr;
        for (int j = 1; (var_name = lua_getlocal(L, &ldb, j)) != nullptr; j++) {
            int ty = lua_type(L, -1);
            if (ty != LUA_TTABLE && ty != LUA_TFUNCTION && var_name[0] != '(') {
                if (!local_out) {
                    errdump << "\n\t\t- local var: ";
                    local_out = true;
                }

                size_t len;
                const char* val = lua_tolstring(L, -1, &len);
                errdump << var_name << " = ";
                if (len > 256) {
                    errdump.write(val, 256);
                    errdump << "...;";
                } else {
                    errdump.write(val, len);
                    errdump << "; ";
                }
            }
            lua_pop(L, 1);
        }
    }

    errdump << '\0';
    LOG_ERR("{}", static_cast<const char*>(errdump.str().c_str()));

    lua_pushstring(L, errmsg); //left the error object, keep the same behavior as pcall.
    return 1;
}

LUX_IMPL(LuaVM, LuaVM)
LUX_CTOR(LuaVM)
LUX_FUNC(LuaVM, Init)
LUX_FUNC(LuaVM, SetRef)
LUX_FUNC(LuaVM, mem)
LUX_FUNC(LuaVM, mem_max)
LUX_FUNC(LuaVM, set_gcstep)
LUX_IMPL_END

LuaVM::LuaVM() :
    gcstep_(256),
    mem_max_(0),
    elapse_time_(0),
    gc_time_(0),
    stack_err_time_(0),
    script_ctt_(nullptr)
{
    L_ = luaL_newstate();
    luaL_openlibs(L_);

    lua_atpanic(L_, ErrorHook);
    lua_pushcfunction(L_, ErrorHook);
    err_hook_pos_ = lua_gettop(L_);
}

LuaVM::~LuaVM() {
    //Call(LuaRef::REF_APP_CLOSE);

    if (script_ctt_) {
        script_ctt_->Terminate();
    }

    for (int i = 0; i < toUType(LuaRef::REF_MAX); ++i) {
        ref_func_[i] = {};
    }

    lua_close(L_);
    L_ = NULL;
}

void LuaVM::Reload() {
    lua_newtable(L_);
    lua_setfield(L_, LUA_REGISTRYINDEX, "_LOADED");
    luaL_openlibs(L_);

    ScriptInit();
}

bool LuaVM::DoFile(const char* file, int ret) {
    if (luaL_loadfile(L_, file) != 0) {
        LOG_ERR("loading script[{}] err: {}", file, lua_tostring(L_, -1));
        return false;
    }

    if (lua_pcall(L_, 0, ret, err_hook_pos_) != 0) {
        LOG_ERR("exec script[{}] err: {}", file, lua_tostring(L_, -1));
        return false;
    }

    return true;
}

bool LuaVM::ScriptInit() {
    LuaStackKeeper kp(L_);
    if (!DoFile(preload_script_.c_str())) {
        return false;
    }

    // enter lua script and call main function Init
    if (!DoFile(main_script_.c_str(), 1)) {
        return false;
    }

    if (!lua_istable(L_, -1)) {
        LOG_ERR("main script doesn't return table!");
        return false;
    }

    lua_pushstring(L_, "init");
    lua_gettable(L_, -2);
    if (!lua_isfunction(L_, -1)) {
        LOG_ERR("main init function not found!");
        return false;
    }
    lua_pushstring(L_, args_.c_str());

    return PCall(1, 0);
}

void LuaVM::Init(const char* args, const char* preload_script, const char* main_script) {
    char* stc = getenv("ScriptTimeCheck");
    int istc = stc ? atoi(stc) : 0;
    if (istc > 0) {
        script_ctt_ = new ScriptTimeCheckThread(L_, istc);
    }

    lux::Register::Instance()->Bind(L_);

    args_ = args;
    preload_script_ = preload_script;
    main_script_ = main_script;

    bool ret = ScriptInit();
    if (!ret) {
        LOG_ERR("App init failed");
        exit(-1);
    }
}

int LuaVM::mem() const {
    int mem = lua_gc(L_, LUA_GCCOUNT, 0);
    return mem;
}

void LuaVM::Update(uint32_t dt_us) {
    uint32_t dtime = (uint32_t)(dt_us * 0.001);

    elapse_time_ += dtime;
    if (elapse_time_ > 100) {
        elapse_time_ = 0;
    }

    int memA = lua_gc(L_, LUA_GCCOUNT, 0);

    if (memA > mem_max_) {
        mem_max_ = memA;
    }

    gc_time_ += dtime;
    if (gc_time_ > 60000) { //60s
        PerfSample("Script gc");
        gc_time_ = 0;

        if (lua_gc(L_, LUA_GCSTEP, gcstep_) == 1) {
            lua_gc(L_, LUA_GCRESTART, 0);
            LOG_LOG("[GC] | Memory, luamem = {}M, maxmem = {}M", ((lua_gc(L_, LUA_GCCOUNT, 0)) >> 10), (mem_max_ >> 10));
        }
    }
}

bool LuaVM::PCall(int nargs, int nrets) {
    bool ok = true;
    int base = lua_gettop(L_) - nargs;

    lua_pushcfunction(L_, ErrorHook);
    lua_insert(L_, base);
    if (script_ctt_) script_ctt_->Enter();
    if (lua_pcall(L_, nargs, nrets, base)) {
        lua_pop(L_, 1);
        ok = false;
    }
    lua_remove(L_, base);
    if (script_ctt_) script_ctt_->Leave();

    return ok;
}

void LuaVM::SetRef(int idx, lux::function fn) {
    if (idx < 0 || idx >= toUType(LuaRef::REF_MAX)) {
        luaL_error(fn.state, "LuaSvr::SetRef ref idx out of range!");
    }
    
    // lux::callback cb(fn);
    // ASSERT(cb);

    ref_func_[idx] = lux::callback_stack(fn);
}

lux::callback_stack& LuaVM::GetRef(LuaRef ref) {
    return ref_func_[toUType(ref)];
}

}
