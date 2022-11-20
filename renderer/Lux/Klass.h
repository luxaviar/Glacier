#pragma once

#include <utility>
#include <string.h>
#include <ctype.h>
#include <type_traits>
#include <algorithm>
#include <vector>
#include "lua.hpp"
#include "Base.h"
#include "Dtor.h"
#include "Common/Log.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace glacier {
namespace lux {

static constexpr char kMethodMetaPrefix[] = "_lux_method_";
static constexpr char kSetterMetaPrefix[] = "_lux_setter_";
static constexpr char kGetterMetaPrefix[] = "_lux_getter_";
static constexpr char kClassMetaPrefix[] = "_lux_class_";

template <typename T>
class Klass {
public:
    typedef struct {
        union {
            T* raw;
            std::shared_ptr<T> shared;
        } ptr;
        bool gc;
        bool shared;
    } Handle;

    template<typename ...Args>
    Klass(const char* name, const char* base_name, BaseCtor<T>* ctor, Dtor<T>* dtor, Args... args) :
        ctor_(ctor),
        dtor_(dtor),
        export_functions_{args...}
    {
        Init(name, base_name);
    }

    template<typename ...Args>
    Klass(const char* name, const char* base_name, BaseFunc<T>* func, Args... args) :
        ctor_(nullptr),
        dtor_(nullptr),
        export_functions_{ func, args... }
    {
        Init(name, base_name);
    }

    template<typename ...Args>
    Klass(const char* name, const char* base_name, BaseCtor<T>* ctor, Args... args) :
        ctor_(ctor),
        dtor_(nullptr),
        export_functions_{args...}
    {
        Init(name, base_name);
    }

    Klass(const char* name, const char* base_name, nullptr_t n) :
        ctor_(nullptr),
        dtor_(nullptr),
        export_functions_{}
    {
        Init(name, base_name);
    }

    void Bind(lua_State* L) {
        ASSERT(self_ == this);

        BindClassLib(L);

        luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
        lua_pushcfunction(L, OpenClassLib);
        lua_setfield(L, -2, class_name_.c_str());
        lua_pop(L, 1);
    }

    static Klass<T>* Instance() {
        ASSERT(self_ != nullptr);
        return self_;
    }

    static int Push(lua_State* L, const T* obj, bool gc = false) {
        return Instance()->InternalPush(L, obj, gc);
    }

    static int Push(lua_State* L, const std::shared_ptr<T>& ptr) {
        return Instance()->InternalPush(L, ptr);
    }

    static T* Check(lua_State* L, int narg) {
        return Instance()->InternalCheck(L, narg);
    }

    static Handle* CheckHandle(lua_State* L, int narg) {
        return Instance()->InternalCheckHandle(L, narg);
    }

private:
    //Klass();  // hide default constructor

    static Klass<T>* self_;

    std::string class_name_;
    std::string base_class_name_;
    BaseCtor<T>* ctor_;
    Dtor<T>* dtor_;
    std::vector<BaseFunc<T>*> export_functions_;
    
    std::string method_meta_;
    std::string getter_meta_;
    std::string setter_meta_;
    std::string class_meta_;
    
    void Init(const char* name, const char* base_name) {
        ASSERT(self_ == nullptr);
        self_ = this;

        class_name_ = name;
        base_class_name_ = base_name;

        method_meta_.append(kMethodMetaPrefix).append(name);
        getter_meta_.append(kGetterMetaPrefix).append(name);
        setter_meta_.append(kSetterMetaPrefix).append(name);
        class_meta_.append(kClassMetaPrefix).append(name);
    }
    
    static int OpenClassLib(lua_State* L) {
        return Instance()->InternalOpenClassLib(L);
    }

    int InternalOpenClassLib(lua_State* L) {
        luaL_getmetatable(L, method_meta_.c_str());
        return 1;
    }

    void BindClassLib(lua_State* L) {
        int top = lua_gettop(L);

        luaL_newmetatable(L, method_meta_.c_str());
        int methods = lua_gettop(L);

        luaL_newmetatable(L, getter_meta_.c_str());
        int getter = lua_gettop(L);

        luaL_newmetatable(L, setter_meta_.c_str());
        int setter = lua_gettop(L);

        luaL_newmetatable(L, class_meta_.c_str());
        int metatable = lua_gettop(L);

        // hide metatable from Lua getmetatable()
        lua_pushvalue(L, methods);
        Set(L, metatable, "__metatable");

        lua_pushcfunction(L, Index);
        Set(L, metatable, "__index");

        lua_pushcfunction(L, NewIndex);
        Set(L, metatable, "__newindex");

        lua_pushcfunction(L, ToString);
        Set(L, metatable, "__tostring");

        lua_pushcfunction(L, GC_T);
        Set(L, metatable, "__gc");

        lua_newtable(L);                // mt for method table
        int method_mt = lua_gettop(L);
        lua_pushcfunction(L, New_T);
        lua_pushvalue(L, -1);           // dup new_T function
        Set(L, methods, "new");         // Add new_T to method table
        Set(L, method_mt, "__call");           // method_mt.__call = new_T

        if (base_class_name_.size() > 0) {
            std::string base_mt_name;
            base_mt_name.append(kMethodMetaPrefix).append(base_class_name_);
            luaL_newmetatable(L, base_mt_name.c_str());

            Set(L, method_mt, "__index");      // mt.__index = base's method table
            
            lua_newtable(L);                // mt for getter table
            int getter_mt = lua_gettop(L);

            base_mt_name.clear();
            base_mt_name.append(kGetterMetaPrefix).append(base_class_name_);
            luaL_newmetatable(L, base_mt_name.c_str());
            
            Set(L, getter_mt, "__index"); // getter_mt.__index = base's getter table
            lua_setmetatable(L, getter);

            lua_newtable(L);                // mt for setter table
            int setter_mt = lua_gettop(L);
            
            base_mt_name.clear();
            base_mt_name.append(kSetterMetaPrefix).append(base_class_name_);
            luaL_newmetatable(L, base_mt_name.c_str());

            Set(L, setter_mt, "__index"); // setter_mt.__index = base's setter table
            lua_setmetatable(L, setter);
        }

        lua_setmetatable(L, methods); //methods's metatable = method_mt

        for (size_t i = 0; i < export_functions_.size() ; i++) {
            auto fp = export_functions_[i];
            if (fp == nullptr) {
                break;
            }

            lua_pushstring(L, fp->name.c_str());
            if (fp->is_prop) {
                lua_pushlightuserdata(L, (void*)fp);
                if (fp->is_setter) {
                    lua_settable(L, setter);
                } else {
                    lua_settable(L, getter);
                }
            } else {
                lua_pushinteger(L, (int)i);
                lua_pushcclosure(L, Thunk, 1);
                lua_settable(L, methods);
            }
        }

        lua_pushstring(L, "gc_disable");
        lua_pushcfunction(L, ReleaseOwnership);
        lua_settable(L, methods);

        lua_pushstring(L, "gc_enable");
        lua_pushcfunction(L, AcquireOwnership);
        lua_settable(L, methods);

        //lua_pop(L, 1);  // drop metatable table
        lua_settop(L, top);
        //return 1; //return methods table
    }

    static int Index(lua_State* L) {
        return Instance()->InternalIndex(L);
    }

    static int NewIndex(lua_State* L) {
        return Instance()->InternalNewIndex(L);
    }

    static int Thunk(lua_State* L) {
        return Instance()->InternalThunk(L);
    }

    static int New_T(lua_State* L) {
        return Instance()->InternalNew(L);
    }

    static int GC_T(lua_State* L) {
        return Instance()->InternalGC(L);
    }

    static int ToString(lua_State* L) {
        return Instance()->InternalToString(L);
    }

    /*! garbage collection metamethod */
    int InternalGC(lua_State* L) {
        Handle* ud = static_cast<Handle*>(lua_touserdata(L, 1));
        if (!ud->gc) return 0;

        if (ud->shared) {
            ud->ptr.shared.reset();
        }
        else {
            T* obj = ud->ptr.raw;
            if (obj) {
                //INFO("delete lua object %s", className);
                if (dtor_) {
                    dtor_->destroy(obj);
                }
                else {
                    delete obj;  // call destructor for T objects
                }
            }
        }
        return 0;
    }

    static int ReleaseOwnership(lua_State* L) {
        Handle* ud = static_cast<Handle*>(lua_touserdata(L, 1));
        if (!ud->shared) {
            ud->gc = false;
        }

        return 1;
    }

    static int AcquireOwnership(lua_State* L) {
        Handle* ud = static_cast<Handle*>(lua_touserdata(L, 1));
        if (!ud->shared) {
            ud->gc = true;
        }
        
        return 1;
    }

    int InternalIndex(lua_State *L) {
        const char* key = lua_tostring(L, 2);
        luaL_getmetatable(L, method_meta_.c_str());
        lua_pushstring(L, key);
        lua_gettable(L, -2);
        if (!lua_isnil(L, -1)) {
            return 1;
        }

        luaL_getmetatable(L, getter_meta_.c_str());
        lua_pushstring(L, key);
        lua_gettable(L, -2);
        if (lua_isnil(L, -1)) {
            return 0;
        }

        BaseFunc<T>* func = static_cast<BaseFunc<T>*>(lua_touserdata(L, -1));

        if (func->is_static) {
            return func->call(L);
        } else {
            T* obj = Check(L, 1);
            //lua_remove(L, 1); //getter doesn't recive any arguments
            return func->call(obj, L);
        }
    }

    int InternalNewIndex(lua_State *L) {
        const char* key = lua_tostring(L, 2);
        luaL_getmetatable(L, setter_meta_.c_str());
        lua_pushstring(L, key);
        lua_gettable(L, -2);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 2); //pop top nil & metatable
            if (lua_istable(L, 1)) {
                lua_rawset(L, 1);
            }
            else {
                int ty = lua_type(L, 1);
                luaL_error(L, "call _newindex failed, expected table but got %s. key: %s", lua_typename(L, ty), key);
            }
            return 0;
        }

        BaseFunc<T>* func = static_cast<BaseFunc<T>*>(lua_touserdata(L, -1));

        if (!func) {
            luaL_error(L, "call _newindex failed, expected userdata but got null");
        }

        if (func->is_static) {
            lua_remove(L, 1); //remove table
            lua_remove(L, 1); //remove key
            return func->call(L);
        } else {
            T* obj = Check(L, 1);
            lua_remove(L, 1); //remove table
            lua_remove(L, 1); //remove key
            return func->call(obj, L);
        }
    }

    /*! push onto the Lua stack a userdata containing a pointer to T object */
    int InternalPush(lua_State* L, const T* obj, bool gc) {
        if (!obj) { lua_pushnil(L); return 0; }

        luaL_getmetatable(L, class_meta_.c_str());
        if (lua_isnil(L, -1)) luaL_error(L, "%s missing metatable", class_name_.c_str());
        int mt = lua_gettop(L);
        FetchSubtable(L, mt, "userdata_ptr", "v");
        int tindex = lua_gettop(L);
        Handle* ud = static_cast<Handle*>(AllocUserdata(L, tindex, (void*)obj, sizeof(Handle)));
        if (ud) { //new ud?
            ud->ptr.raw = (T*)obj;  // store pointer to object in userdata
            ud->gc = gc;
            ud->shared = false;
            lua_pushvalue(L, mt);
            lua_setmetatable(L, -2);
        }
        lua_replace(L, mt); //set ud -> mt position
        lua_settop(L, mt); //now ud is top
        return mt;  // index of userdata containing pointer to T object
    }

    int InternalPush(lua_State* L, const std::shared_ptr<T>& ptr) {
        if (!ptr) { lua_pushnil(L); return 0; }

        luaL_getmetatable(L, class_meta_.c_str());
        if (lua_isnil(L, -1)) luaL_error(L, "%s missing metatable", class_name_.c_str());
        int mt = lua_gettop(L);
        FetchSubtable(L, mt, "userdata_shared", "v");

        T* obj = ptr.get();
        int tindex = lua_gettop(L);
        Handle* ud = static_cast<Handle*>(AllocUserdata(L, tindex, (void*)obj, sizeof(Handle)));
        if (ud) { //new ud?
            new(&ud->ptr) std::shared_ptr<T>(ptr);
            ud->gc = true;
            ud->shared = true;
            lua_pushvalue(L, mt);
            lua_setmetatable(L, -2);
        }
        lua_replace(L, mt);
        lua_settop(L, mt);
        return mt;  // index of userdata containing pointer to T object
    }

    /*! get userdata from Lua stack and return pointer to T object */
    T* InternalCheck(lua_State* L, int narg) {
        Handle* ud = InternalCheckHandle(L, narg);
        if (!ud) {
            return nullptr;
        }

        return ud->shared ? ud->ptr.shared.get() : ud->ptr.raw;
    }

    /*! get userdata from Lua stack*/
    Handle* InternalCheckHandle(lua_State* L, int narg) {
        if (!lua_getmetatable(L, narg)) {
            //luaL_typerror(L, narg, className);
            const char* msg = lua_pushfstring(L, "%s expected, got %s",
                class_name_.c_str(), luaL_typename(L, narg));
            luaL_argerror(L, narg, msg);

            return nullptr;
        }
        lua_pop(L, 1);

        Handle* ud = static_cast<Handle*>(lua_touserdata(L, narg));
        if (!ud) {
            lua_getfield(L, narg, "_core");
            ud = static_cast<Handle*>(lua_touserdata(L, -1));
            lua_pop(L, 1);
            if (!ud) {
                luaL_error(L, "it's a member function, need self ptr");
                return nullptr;
            }
        }

        return ud;
    }

    int InternalThunk(lua_State* L) {
        int i = (int)lua_tonumber(L, lua_upvalueindex(1)); // which function?
        auto fp = export_functions_[i];
        if (fp->is_static) {
            return fp->call(L);
        } else {
            T* obj = Check(L, 1);
            lua_remove(L, 1);
            return fp->call(obj, L);
        }
    }

    // create a new T object and
    // push onto the Lua stack a userdata containing a pointer to T object
    int InternalNew(lua_State* L) {
        if (!ctor_) return 0;

        lua_remove(L, 1);   // use classname:new(), instead of classname.new()
        T* obj = ctor_->spawn(L);
        Push(L, obj, true); // gc_T will delete this object
        return 1;           // userdata containing pointer to T object
    }

    int InternalToString(lua_State * L) {
        char buff[32];
        Handle* ud = static_cast<Handle*>(lua_touserdata(L, 1));
        T* obj = ud->shared ? ud->ptr.shared.get() : ud->ptr.raw;
        snprintf(buff, sizeof(buff), "%p", (void*)obj);
        lua_pushfstring(L, "%s (%s)", class_name_.c_str(), buff);
        return 1;
    }

    static void Set(lua_State* L, int table_index, const char* key) {
        lua_pushstring(L, key);
        lua_insert(L, -2);  // swap value and key
        lua_settable(L, table_index);
    }

    static void NewWeaktable(lua_State* L, const char* mode) {
        lua_newtable(L);
        lua_pushvalue(L, -1);  // table is its own metatable
        lua_setmetatable(L, -2);
        lua_pushliteral(L, "__mode");
        lua_pushstring(L, mode);
        lua_settable(L, -3);   // metatable.__mode = mode
    }

    static void FetchSubtable(lua_State* L, int tindex, const char* name, const char* mode) {
        lua_pushstring(L, name);
        lua_gettable(L, tindex);
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_checkstack(L, 3);
            NewWeaktable(L, mode);
            lua_pushstring(L, name);
            lua_pushvalue(L, -2);
            lua_settable(L, tindex);
        }
    }

    static void* AllocUserdata(lua_State* L, int tindex, void* key, size_t sz) {
        void* ud = nullptr;
        lua_pushlightuserdata(L, key);
        lua_gettable(L, tindex);     // lookup[key]
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1);         // drop nil
            lua_checkstack(L, 3);
            ud = lua_newuserdata(L, sz);  // create new userdata
            lua_pushlightuserdata(L, key);
            lua_pushvalue(L, -2);  // dup userdata
            lua_settable(L, tindex);   // lookup[key] = userdata
        }
        return ud;
    }
};

template<typename T>
Klass<T>* Klass<T>::self_ = nullptr;

}
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
