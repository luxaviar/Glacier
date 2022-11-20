#include "Monitor.h"

namespace glacier {

static uint32_t kMaxScriptTime = 15;

ScriptTimeCheckThread::ScriptTimeCheckThread(lua_State* L, int istc) {
    L_ = L;
    enterTime_ = 0;
    level_ = 0;
    break_ = 0;

    if (istc > 0) {
        kMaxScriptTime = istc;
    }

    thread_ = new std::thread(&ScriptTimeCheckThread::Work, this);
}

ScriptTimeCheckThread::~ScriptTimeCheckThread() {
    thread_->join();
}

void ScriptTimeCheckThread::Enter() {
    if (level_ == 0) enterTime_ = time(NULL);
    level_++;
}

void ScriptTimeCheckThread::Leave() {
    level_--;
    if (level_ == 0) enterTime_ = 0;
}

// this function called in script thread
void ScriptTimeCheckThread::TimeoutBreak(lua_State* L, lua_Debug* D) {
    lua_sethook(L, NULL, 0, 0);
    luaL_error(L, "Script timeout.");
}

// this function called in check thread
void ScriptTimeCheckThread::Work() {
    while (true) {
        Sleep(5 * 1000);
        if (enterTime_ && kMaxScriptTime && time(NULL) - enterTime_ > kMaxScriptTime) {
            int mask = LUA_MASKCALL | LUA_MASKRET | LUA_MASKLINE | LUA_MASKCOUNT;
            //FIXME: not threadsafe!!!
            lua_sethook(L_, TimeoutBreak, mask, 1);
            enterTime_ = 0;
        }

        if (break_) break;
    }
}

}
