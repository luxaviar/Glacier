#pragma once

#include <string.h>
#include <map>
#include <functional>
#include <vector>
#include "lua.hpp"
#include "Stack.h"
#include "Common/Singleton.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

namespace glacier {
namespace lux {

struct Register : public Singleton<Register> {
    using BinderFunction = std::function<void(lua_State*)>;

    struct PreloadLib {
        PreloadLib(const char* name_, lua_CFunction fn_);
        void Bind(lua_State* L);

        std::string name;
        lua_CFunction fn;
    };

    struct Function {
        Function(const char* name_, lua_CFunction fn_);

        std::string name;
        lua_CFunction fn;
    };

    template<typename T>
    struct Klass {
        template<typename ...Args>
        Klass(Args&&... args) {
            assert(instance_ == nullptr);
            instance_ = new glacier::lux::Klass<T>(std::forward<Args>(args)...);
            BinderFunction func = std::bind(&glacier::lux::Klass<T>::Bind, instance_, std::placeholders::_1);
            Register::Instance()->AddKlass(func);
        }

        static glacier::lux::Klass<T>* instance_;
    };

    struct ConstantLoader {
        virtual int Load(lua_State* L) = 0;
        std::string name;
    };

    template<typename T, typename=void>
    struct ConstantValue {
        using type = T;
        using lua_type = T;
        static type to_value(T v) { return v; }
        static lua_type to_lua_value(T v) { return v; }
    };

    template<typename T>
    struct Constant : public ConstantLoader {
        using value_type = typename ConstantValue<T>::type;

        Constant(const char* name_, T v) : single(true), value(v) {
            name = name_;
            Register::Instance()->AddConstant(this);
        }

        template<typename ...Args>
        Constant(const char* name_, const char* key, T v, Args... args) : single(false) {
            name = name_;
            AddKeyValue(key, v);
            AddKeyValue(args...);
            Register::Instance()->AddConstant(this);
        }

        void AddKeyValue(nullptr_t p) {
        }

        void AddKeyValue(const char* key, T v) {
            key_value.emplace_back(key, ConstantValue<T>::to_value(v));
        }

        template<typename ...Args>
        void AddKeyValue(const char* key, T v, Args... args) {
            AddKeyValue(key, v);
            AddKeyValue(args...);
        }

        int Load(lua_State* L) {
            if (single) {
                lux::push(L, ConstantValue<T>::to_lua_value(value));
            } else {
                lua_newtable(L);
                int idx = lua_gettop(L);
                for (auto& pair : key_value) {
                    lux::set_table(L, idx, pair.first.c_str(), ConstantValue<T>::to_lua_value(pair.second));
                }
            }
            return 1;
        }

        bool single;
        value_type value;
        std::vector<std::pair<std::string, value_type>> key_value;
    };

    void Bind(lua_State* L);

    void BindFunction(Function* func, lua_State* L);
    void AddFunction(Function* func);

    void BindConstant(ConstantLoader* loader, lua_State* L);
    void AddConstant(ConstantLoader* loader);

    void AddPreloadLib(BinderFunction fn);
    void AddKlass(BinderFunction fn);

private:
    static int LoadConstant(lua_State* L);
    static int LoadFunction(lua_State* L);
    //static int Load(lua_State* L);

    static Register* self_;

    std::map<std::string, Function*> functions_;
    std::map<std::string, ConstantLoader*> constants_;
    std::vector<BinderFunction> klass_list_;
    std::vector<BinderFunction> binder_list_;
};

template<typename T>
Klass<T>* Register::Klass<T>::instance_ = nullptr;

template<>
struct Register::ConstantValue<char*, void> {
    using type = std::string;
    using lua_type = char*;
    static type to_value(char* v) { return v; }
    static char* to_lua_value(const type& v) { return (char*)v.c_str(); }
};

template<>
struct Register::ConstantValue<const char*, void> {
    using type = std::string;
    using lua_type = const char*;
    static type to_value(const char* v) { return v; }
    static const char* to_lua_value(const type& v) { return v.c_str(); }
};

template<typename T>
struct Register::ConstantValue<T, typename std::enable_if_t<std::is_enum<T>::value>> {
    using type = typename std::underlying_type<T>::type;
    using lua_type = typename std::underlying_type<T>::type;
    static type to_value(T v) { return toUType(v); }
    static lua_type to_lua_value(type v) { return v; }
};

}
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
