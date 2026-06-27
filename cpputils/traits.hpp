#pragma once

#include "common.hpp"

namespace traits {

struct false_type {
    static constexpr bool value = false;
    constexpr operator bool() const noexcept {return value;}
};

struct true_type {
    static constexpr bool value = true;
    constexpr operator bool() const noexcept {return value;}
};

// All types sans the explicit specializations are not integers.
template<class T> struct integer  : false_type {};
template<class T> struct floating : false_type {};

// Char is not guaranteed to be signed nor unsigned.
template<> struct integer<char> : true_type {};

// Unsigned types.
template<> struct integer<u8>  : true_type {};
template<> struct integer<u16> : true_type {};
template<> struct integer<u32> : true_type {};
template<> struct integer<u64> : true_type {};

// Signed types.
template<> struct integer<i8>  : true_type {};
template<> struct integer<i16> : true_type {};
template<> struct integer<i32> : true_type {};
template<> struct integer<i64> : true_type {};

// Flaating-point types.
template<> struct floating<f32> : true_type {};
template<> struct floating<f64> : true_type {};

template<class T>
constexpr bool
is_integer = integer<T>::value;

template<class T>
constexpr bool
is_floating = floating<T>::value;

template<class T>
constexpr bool
is_numeric = is_integer<T> || is_floating<T>;
};

