#pragma once

#include "Base.h"
#include "Stack.h"

namespace glacier {
namespace lux {

template<typename T, typename ...Args>
class Ctor : public BaseCtor<T> {
public:
    constexpr Ctor() noexcept {}

    template<size_t ...N>
    T* apply(lua_State* L, indices<N...>) const {
        return new T(get_arg<Args>(L, N + 1)...);
    }

    T* spawn(lua_State* L) const override {
        constexpr size_t args_num = sizeof...(Args);
        return apply(L, typename indices_builder<args_num>::type());
    }
};

template<typename T>
class Ctor<T, lua_State*> : public BaseCtor<T> {
public:
    constexpr Ctor() noexcept {}

    T* apply(lua_State* L) const {
        return new T(L);
    }

    T* spawn(lua_State* L) const override {
        return apply(L);
    }
};

}
}
