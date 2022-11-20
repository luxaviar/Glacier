#pragma once

#include <functional>
#include "Base.h"
#include "Stack.h"

namespace glacier {
namespace lux {

template<typename... Args, size_t ...N>
int _apply(lua_State* L, void(*fun)(Args...), indices<N...>) {
    fun(get_arg<Args>(L, N + 1)...);
    return 0;
}

template<typename Ret, typename... Args, size_t ...N>
int _apply(lua_State* L, Ret(*fun)(Args...), indices<N...>) {
    return ret(L, fun(get_arg<Args>(L, N + 1)...));
}

template<typename T, typename... Args, size_t... N>
int _apply(lua_State* L, const std::function<void(T*, Args...)>& fun, T* t, indices<N...>) {
    fun(t, get_arg<Args>(L, N + 1)...);
    return 0;
}

template<typename T, typename Ret, typename... Args, size_t... N>
int _apply(lua_State* L, const std::function<Ret(T*, Args...)>& fun, T* t, indices<N...>) {
    return ret(L, fun(t, get_arg<Args>(L, N + 1)...));
}

template<typename T, typename P, typename... Args, size_t... N>
int _apply_member(lua_State* L, void(P::* fun)(Args...), T* t, indices<N...>) {
    (t->*fun)(get_arg<Args>(L, N + 1)...);
    return 0;
}

template<typename T, typename P, typename Ret, typename... Args, size_t... N>
int _apply_member(lua_State* L, Ret(P::* fun)(Args...), T* t, indices<N...>) {
    return ret(L, (t->*fun)(get_arg<Args>(L, N + 1)...));
}

template<typename T, typename P, typename... Args, size_t... N>
int _apply_member(lua_State* L, void(P::* fun)(Args...) const, T* t, indices<N...>) {
    (t->*fun)(get_arg<Args>(L, N + 1)...);
    return 0;
}

template<typename T, typename P, typename Ret, typename... Args, size_t... N>
int _apply_member(lua_State* L, Ret(P::* fun)(Args...) const, T* t, indices<N...>) {
    return ret(L, (t->*fun)(get_arg<Args>(L, N + 1)...));
}

template<typename T>
struct StdClassFunc : public BaseFunc<T> {
    using _std_fun_type = int(*)(lua_State*);
    _std_fun_type fun;

    StdClassFunc(const char* name, int(*fun_)(lua_State*)) : BaseFunc<T>(true, true, name) {
        this->fun = fun_;
    }

    int call(lua_State* L) const {
        return this->fun(L);
    }
};

template<typename T, typename P,
    typename std::enable_if_t<std::is_base_of<P, T>::value || std::is_same<P, T>::value, int> = 0>
struct StdMemberFunc : public BaseFunc<T> {
    using _herit_std_method_type = int(P::*)(lua_State*);
    _herit_std_method_type fun;

    StdMemberFunc(const char* name, int(P::* fun_)(lua_State*)) : BaseFunc<T>(false, true, name) {
        this->fun = fun_;
    }

    int call(T* t, lua_State* L) const {
        return (t->*fun)(L);
    }
};

template<typename Ret, typename T, typename ...Args>
struct ClassFunc : public BaseFunc<T> {
    using _fun_type = Ret(*)(Args...);
    _fun_type fun;

    ClassFunc(const char* name, Ret(*fun_)(Args...)) : BaseFunc<T>(true, false, name), fun(fun_) {}

    int call(lua_State* L) const {
        constexpr size_t args_num = sizeof...(Args);
        return _apply(L, fun, typename indices_builder<args_num>::type());
    }
};

template<typename Ret, typename T, typename P, typename ...Args>
struct MemberFunc : public BaseFunc<T> {
    constexpr static typename std::enable_if_t<std::is_base_of<P, T>::value || std::is_same<P, T>::value>* valid = nullptr;
    using _fun_type = Ret(P::*)(Args...);
    _fun_type fun;

    MemberFunc(const char* name, Ret(P::* fun_)(Args...)) : BaseFunc<T>(false, false, name) {
        fun = fun_;
    }

    int call(T* t, lua_State* L) const {
        constexpr size_t args_num = sizeof...(Args);
        return _apply_member(L, fun, t, typename indices_builder<args_num>::type());
    }
};

template<typename Ret, typename T, typename P, typename ...Args>
struct MemberConstFunc : public BaseFunc<T> {
    constexpr static typename std::enable_if_t<std::is_base_of<P, T>::value || std::is_same<P, T>::value>* valid = nullptr;
    using _fun_type = Ret(P::*)(Args...) const;
    _fun_type fun;

    MemberConstFunc(const char* name, Ret(P::* fun_)(Args...) const) : BaseFunc<T>(false, false, name) {
        fun = fun_;
    }

    int call(T* t, lua_State* L) const {
        constexpr size_t args_num = sizeof...(Args);
        return _apply_member(L, fun, t, typename indices_builder<args_num>::type());
    }
};

template<typename T, typename P, typename M,
    typename std::enable_if_t<std::is_base_of<P, T>::value || std::is_same<P, T>::value, int> = 0>
struct MemberPropGet : public BaseFunc<T> {
    std::function<M(T*)> get_fun;

    MemberPropGet(const char* name_, M P::* member) : BaseFunc<T>(false, false, name_, true) {
        get_fun = [member](T* t) -> M {
            return t->*member;
        };
    }

    int call(T* t, lua_State* L) const override {
        M m = get_fun(t);
        return ret(L, m);
    }
};

template<typename T, typename P, typename M,
    typename std::enable_if_t<std::is_base_of<P, T>::value || std::is_same<P, T>::value, int> = 0>
struct MemberPropGetFunc : public BaseFunc<T> {
    using _func_type = M(P::*)();
    _func_type fun;

    MemberPropGetFunc(const char* name_, M(P::* fun_)()) : BaseFunc<T>(false, false, name_, true) {
        fun = fun_;
    }

    int call(T* t, lua_State* L) const override {
        M m = (t->*fun)();
        return ret(L, m);
    }
};

template<typename T, typename P, typename M,
    typename std::enable_if_t<std::is_base_of<P, T>::value || std::is_same<P, T>::value, int> = 0>
struct MemberPropGetFuncConst : public BaseFunc<T> {
    using _func_type = M(P::*)() const;
    _func_type fun;

    MemberPropGetFuncConst(const char* name_, M(P::* fun_)() const) : BaseFunc<T>(false, false, name_, true) {
        fun = fun_;
    }

    int call(T* t, lua_State* L) const override {
        M m = (t->*fun)();
        return ret(L, m);
    }
};

template<typename T, typename P, typename M,
    typename std::enable_if_t<std::is_base_of<P, T>::value || std::is_same<P, T>::value, int> = 0>
struct MemberPropSet : public BaseFunc<T> {
    std::function<void(T*, M)> set_fun;

    MemberPropSet(const char* name_, M P::* member) : BaseFunc<T>(false, false, name_, true, true) {
        set_fun = [member](T* t, M m) {
            t->*member = m;
        };
    }

    int call(T* t, lua_State* L) const override {
        M m = get_arg<M>(L, 1);
        set_fun(t, m);
        return 0;
    }
};

template<typename T, typename P, typename M,
    typename std::enable_if_t<std::is_base_of<P, T>::value || std::is_same<P, T>::value, int> = 0>
struct MemberPropSetFunc : public BaseFunc<T> {
    using _func_type = void(P::*)(M);
    _func_type fun;

    MemberPropSetFunc(const char* name_, void(P::* fun_)(M)) : BaseFunc<T>(false, false, name_, true, true) {
        fun = fun_;
    }

    int call(T* t, lua_State* L) const override {
        M m = get_arg<M>(L, 1);
        (t->*fun)(m);
        return 0;
    }
};

template<typename T, typename M>
struct ClassPropGet : public BaseFunc<T> {
    M* prop;

    ClassPropGet(const char* name_, M* member) : BaseFunc<T>(true, false, name_, true), prop(member) {
    }

    int call(lua_State* L) const override {
        return ret(L, *prop);
    }
};

template<typename T, typename M>
struct ClassPropGetFunc : public BaseFunc<T> {
    using _func_type = M(*)();
    _func_type fun;

    ClassPropGetFunc(const char* name_, M(*fun_)()) : BaseFunc<T>(true, false, name_, true, false), fun(fun_) {
    }

    int call(lua_State* L) const override {
        M m = fun();
        return ret(L, m);
    }
};

template<typename T, typename M>
struct ClassPropSet : public BaseFunc<T> {
    M* prop;

    ClassPropSet(const char* name_, M* member) : BaseFunc<T>(true, false, name_, true, true), prop(member) {
    }

    int call(lua_State* L) const override {
        M m = get_arg<M>(L, 1);
        *prop = m;
        return 0;
    }
};

template<typename T, typename M>
struct ClassPropSetFunc : public BaseFunc<T> {
    using _func_type = void(*)(M);
    _func_type fun;

    ClassPropSetFunc(const char* name_, void(*fun_)(M)) : BaseFunc<T>(true, false, name_, true, true), fun(fun_) {
    }

    int call(lua_State* L) const override {
        M m = get_arg<M>(L, 1);
        fun(m);
        return 0;
    }
};

template<typename T, typename Ret, typename ...Args>
struct spec_func_generator {
    static BaseFunc<T>* gen(const char* name, Ret(T::* fun)(Args...), std::true_type) {
        return new MemberFunc<Ret, T, T, Args...>(name, fun);
    }

    static BaseFunc<T>* gen(const char* name, Ret(*fun)(Args...), std::false_type) {
        return new ClassFunc<Ret, T, Args...>(name, fun);
    }
};

template<typename T>
struct spec_func_generator<T, int, lua_State*> {
    static BaseFunc<T>* gen(const char* name, int(T::* fun)(lua_State*), std::true_type) {
        return new StdMemberFunc<T, T>(name, fun);
    }

    static BaseFunc<T>* gen(const char* name, int(*fun)(lua_State*), std::false_type) {
        return new StdClassFunc<T>(name, fun);
    }
};

template<typename T>
struct export_helper {
    static BaseFunc<T>* gen(const char* name, int(*fun)(lua_State*)) {
        return new StdClassFunc<T>(name, fun);
    }

    template<typename Ret, typename ...Args>
    static BaseFunc<T>* gen(const char* name, Ret(*fun)(Args...)) {
        return new ClassFunc<Ret, T, Args...>(name, fun);
    }

    template<typename P>
    static BaseFunc<T>* gen(const char* name, int(P::* fun)(lua_State*)) {
        return new StdMemberFunc<T, P>(name, fun);
    }

    template<typename P, typename Ret, typename ...Args>
    static BaseFunc<T>* gen(const char* name, Ret(P::* fun)(Args...)) {
        return new MemberFunc<Ret, T, P, Args...>(name, fun);
    }

    template<typename P, typename Ret, typename ...Args>
    static BaseFunc<T>* gen(const char* name, Ret(P::* fun)(Args...) const) {
        return new MemberConstFunc<Ret, T, P, Args...>(name, fun);
    }

    template<typename M>
    static BaseFunc<T>* gen_getter(const char* name, M* member) {
        return new ClassPropGet<T, M>(name, member);
    }

    template<typename M>
    static BaseFunc<T>* gen_getter_func(const char* name, M(*fun)()) {
        return new ClassPropGetFunc<T, M>(name, fun);
    }

    template<typename M>
    static BaseFunc<T>* gen_setter(const char* name, M* member) {
        return new ClassPropSet<T, M>(name, member);
    }

    template<typename M>
    static BaseFunc<T>* gen_setter_func(const char* name, void(*fun)(M)) {
        return new ClassPropSetFunc<T, M>(name, fun);
    }

    template<typename P, typename M>
    static BaseFunc<T>* gen_getter(const char* name, M P::* member) {
        return new MemberPropGet<T, P, M>(name, member);
    }

    template<typename P, typename M>
    static BaseFunc<T>* gen_getter_func(const char* name, M(P::* fun)()) {
        return new MemberPropGetFunc<T, P, M>(name, fun);
    }

    template<typename P, typename M>
    static BaseFunc<T>* gen_getter_func(const char* name, M(P::* fun)() const) {
        return new MemberPropGetFuncConst<T, P, M>(name, fun);
    }

    template<typename P, typename M>
    static BaseFunc<T>* gen_setter(const char* name, M P::* member) {
        return new MemberPropSet<T, P, M>(name, member);
    }

    template<typename P, typename M>
    static BaseFunc<T>* gen_setter_func(const char* name, void(P::* fun)(M)) {
        return new MemberPropSetFunc<T, P, M>(name, fun);
    }
};

}
}
