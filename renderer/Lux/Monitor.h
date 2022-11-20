#pragma once

#include <thread>
#include <time.h>
#include "lua.hpp"

namespace glacier {

class ScriptTimeCheckThread {
public:
    // this function called in script thread
    static void TimeoutBreak(lua_State* L, lua_Debug* D);

    ScriptTimeCheckThread(lua_State* L, int istc = 0);
    ~ScriptTimeCheckThread();

    void Enter();
    void Leave();

    // this function called in check thread
    void Work();
    void Terminate() { break_ = 1; }
private:
    uint32_t enterTime_;
    uint32_t level_;
    uint32_t break_;

    lua_State* L_;
    std::thread* thread_;
};

}
