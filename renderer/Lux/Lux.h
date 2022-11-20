#pragma once

#include <vector>
#include "Klass.h"
#include "Ctor.h"
#include "Dtor.h"
#include "Func.h"
#include "Register.h"

#define LUX_FULL_CLASS_NAME(name) "Glacier." #name

#define LUX_STATIC_KLASS_VAR(name) glacier_lux_static_klass_reg_var_##name

#define LUX_IMPL_INHERIT(klass, name, base_name) static glacier::lux::Register::Klass<klass> LUX_STATIC_KLASS_VAR(klass)(LUX_FULL_CLASS_NAME(name), LUX_FULL_CLASS_NAME(base_name),
#define LUX_IMPL_SIMPLE(klass, name) static glacier::lux::Register::Klass<klass> LUX_STATIC_KLASS_VAR(klass)(LUX_FULL_CLASS_NAME(name), "",

#define LUX_FUNC_SIGNED(klass, name, ret, ...) glacier::lux::spec_func_generator<klass, ret, __VA_ARGS__>::gen(#name, &klass::name, std::is_member_pointer<decltype(static_cast<ret (klass::*)(__VA_ARGS__)>(&klass::name))>::type{}),
#define LUX_FUNC_SIMPLE(klass, name) glacier::lux::export_helper<klass>::gen(#name, &klass::name),

#define LUX_FUNC_SPEC(klass, name, new_name, ret, ...) glacier::lux::spec_func_generator<klass, ret, __VA_ARGS__>::gen(#new_name, &klass::name, std::is_member_pointer<decltype(static_cast<ret (klass::*)(__VA_ARGS__)>(&klass::name))>::type{}),

#define LUX_ARG_16TH(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, ...) _16
#define LUX_ARG_4TH(_1, _2, _3, _4, ...) _4
#define LUX_EXPAND_VA_ARG(...)  __VA_ARGS__

#define LUX_FUNC_NAME(s) LUX_FUNC_##s
#define LUX_FUNC_NAME_HELPER(s) LUX_FUNC_NAME(s)

#ifdef _MSC_VER

#define LUX_EAT_COMMA(...) ,##__VA_ARGS__

#define LUX_IMPL_MACRO_CHOOSER(...) LUX_EXPAND_VA_ARG(LUX_ARG_4TH(__VA_ARGS__, LUX_IMPL_INHERIT, LUX_IMPL_SIMPLE))
#define LUX_IMPL(...) LUX_EXPAND_VA_ARG(LUX_IMPL_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__))

#define LUX_CTOR(klass, ...) new glacier::lux::Ctor<klass LUX_EAT_COMMA(__VA_ARGS__)>,

#define LUX_FUNC_SUFFIX(...) LUX_EXPAND_VA_ARG(LUX_ARG_16TH(__VA_ARGS__, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIMPLE))
#define LUX_FUNC_MACRO_CHOOSER(...) LUX_FUNC_NAME_HELPER(LUX_FUNC_SUFFIX(__VA_ARGS__))

#define LUX_FUNC(...) LUX_EXPAND_VA_ARG(LUX_FUNC_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__))

#else

#define LUX_IMPL_MACRO_CHOOSER(...) LUX_ARG_4TH(__VA_ARGS__, LUX_IMPL_INHERIT, LUX_IMPL_SIMPLE)
#define LUX_IMPL(...) LUX_IMPL_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#define LUX_CTOR(klass, ...) new glacier::lux::Ctor<klass, ##__VA_ARGS__>,

#define LUX_FUNC_SUFFIX(...) LUX_ARG_16TH(__VA_ARGS__, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIGNED, SIMPLE)
#define LUX_FUNC_MACRO_CHOOSER(...) LUX_FUNC_NAME_HELPER(LUX_FUNC_SUFFIX(__VA_ARGS__))

#define LUX_FUNC(...) LUX_FUNC_MACRO_CHOOSER(__VA_ARGS__)(__VA_ARGS__)

#endif

#define LUX_DTOR(klass, func) new glacier::lux::Dtor<klass>(&klass::func),

#define LUX_PROP(klass, name) glacier::lux::export_helper<klass>::gen_getter(#name, &klass::name), \
        glacier::lux::export_helper<klass>::gen_setter(#name, &klass::name), 

#define LUX_PROP_FUNC(klass, name) glacier::lux::export_helper<klass>::gen_getter_func(#name, &klass::name), \
        glacier::lux::export_helper<klass>::gen_setter_func(#name, &klass::name), 

#define LUX_PROP_FUNC_GET(klass, name, func) glacier::lux::export_helper<klass>::gen_getter_func(#name, &klass::func),

#define LUX_PROP_FUNC_SET(klass, name, func, param) glacier::lux::export_helper<klass>::gen_setter_func(#name, static_cast<void (klass::*)(param)>(&klass::func)),

#define LUX_IMPL_END nullptr);

#define LUX_FULL_FUNCTION_NAME(name) "Glacier." #name
#define LUX_FULL_CONSTANT_NAME(name) "Glacier." #name

#define LUX_GLOBAL_FUNC_VAR(name) glacier_lux_GLOBAL_FUNCtion_reg_var_##name
#define LUX_GLOBAL_FUNC(name, func) static glacier::lux::Register::Function LUX_GLOBAL_FUNC_VAR(name)(LUX_FULL_FUNCTION_NAME(name), func);

#define LUX_PRELOAD_LIB_VAR(name) glacier_lux_preloadlib_reg_var_##name
#define LUX_PRELOAD_LIB(name, func) static glacier::lux::Register::PreloadLib LUX_PRELOAD_LIB_VAR(name)(#name, func);

#define LUX_CONSTANT_VAR(name) glacier_lux_constant_reg_var_##name
#define LUX_CONSTANT(name, value) static glacier::lux::Register::Constant<decltype(value)> LUX_CONSTANT_VAR(name)(LUX_FULL_CONSTANT_NAME(name), value)
#define LUX_CONSTANT_MULTI(name, type) static glacier::lux::Register::Constant<type> LUX_CONSTANT_VAR(type)(LUX_FULL_CONSTANT_NAME(name),
#define LUX_CONST(key, value) key, value,
#define LUX_CONSTANT_MULTI_END nullptr);
