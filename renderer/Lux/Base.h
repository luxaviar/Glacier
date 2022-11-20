#pragma once

#include <string>

namespace glacier {
namespace lux {

template<size_t ...Idx>
struct indices {};

template<size_t N, size_t ...Idx>
struct indices_builder : indices_builder<N - 1, N - 1, Idx...> {};

template<size_t ...Idx>
struct indices_builder<0, Idx...> {
    using type = indices<Idx...>;
};

template<typename T>
struct BaseFunc {
    BaseFunc(bool is_static_, bool is_std_, const char* name_, bool is_prop_=false, bool is_setter_=false) :
        is_static(is_static_),
        is_std(is_std_),
        is_prop(is_prop_),
        is_setter(is_setter_),
        name(name_) {
    }

    virtual ~BaseFunc() {};
    virtual int call(T* t, lua_State* L) const { return 0; };
    virtual int call(lua_State* L) const { return 0; };

    bool is_static;
    bool is_std;
    bool is_prop;
    bool is_setter;
    std::string name;
};

template<typename T>
class BaseCtor {
public:
    virtual T* spawn(lua_State* L) const { return nullptr; }
};

}
}
