#pragma once

#include <type_traits>
#include <stddef.h>

namespace glacier {

template<size_t N>
constexpr size_t calc_length(char const(&)[N]) {
    return N - 1;
}

template<typename E>
constexpr typename std::underlying_type<E>::type toUType(E enumerator) noexcept {
    return static_cast<typename std::underlying_type<E>::type>(enumerator);
}

template <typename> 
struct is_template : std::false_type {};

template <template <typename...> class Tmpl, typename ...Args>
struct is_template<Tmpl<Args...>> : std::true_type {};

}
