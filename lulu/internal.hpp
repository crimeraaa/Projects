#pragma once

// standard
#include <cstddef>   // size_t
#include <cstdint>   //  u?int\d+_t
#include <cmath>     // floor

#if defined(__GNUC__) || defined(__clang__)

#define LULU_UNREACHABLE()  __builtin_trap()
#define LULU__ASSERT_IMPL() __builtin_trap()
#define restrict            __restrict__

#elif defined(_MSC_VER) // ^^^ GCC, clang; vvv MSVC

#define LULU__ASSERT_IMPL() __debugbreak()
#define LULU_UNREACHABLE()  cast(void)0
#define LULU_FORMAT(f, a)
#define restrict            __restrict

#else // ^^^ MSVC ; vvv <unknown>

#include <cstdlib>

#define LULU_FORMAT(f, a)
#define LULU_UNREACHABLE()  cast(void)0
#define restrict

// Always works but may not be good for debuggers.
#define LULU__ASSERT_IMPL() std::abort()
#endif

#define cast(T)         (T)
#define unused(expr)    cast(void)(expr)
#define count_of(expr)  (sizeof(expr) / sizeof((expr)[0]))

#if 1
#include <cstdio> // fprintf
#define LULU_LOGF(fmt, ...) \
    std::fprintf(stderr, "%s:%i: " fmt "\n", __func__, __LINE__, __VA_ARGS__)

#else

#define LULU_LOGF(fmt, ...)   cast(void)0

#endif // LULU_LOGF
#define LULU_LOGLN(msg) LULU_LOGF("%s", msg)


// TODO(2026-07-03): Make configurable?
#define LULU_USE_ASSERT 1

/*
 NOTE(2026-07-03):
    For your sanity, ensure that the expression does NOT have any side-effects.
    Otherwise, when assertions are disabled, you WILL get strange behavior.
 */
#if LULU_USE_ASSERT

#define LULU_ASSERTF(expr, fmt, ...)                                           \
    (cast(bool)(expr)                                                          \
        ? cast(void)0                                                          \
        : (LULU_LOGF(fmt, __VA_ARGS__), LULU__ASSERT_IMPL()))                  \

#else // ^^^ LULU_USE_ASSERT | vvv LULU_USE_ASSERT

#define LULU_ASSERTF(expr, fmt, ...)    cast(void)0

#endif // LULU_USE_ASSERT

#define LULU_ASSERTLN(e, msg)   LULU_ASSERTF (e, "%s", msg)
#define LULU_ASSERT(e)          LULU_ASSERTLN(e, "Assertion failed: '" #e "'")
#define LULU_UNIMPLEMENTED()    LULU_ASSERTLN(false, "Unimplemented")
#define LULU_PANICF(fmt, ...)   LULU_ASSERTF (false, fmt, __VA_ARGS__)
#define LULU_PANICLN(msg)       LULU_PANICF  ("%s", msg)
#define LULU_PANIC()            LULU_PANICLN ("Runtime panic")

// Fixed-size unsigned integer types.
using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

// Fixed-size signed integer types.
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

// Fixed-size floating point types.
using f32 = float;
using f64 = double;

// Convenience types.
using usize   = std::size_t;
using uintptr = std::uintptr_t;

// TODO(2026-07-08): Make configurable?
using lulu_int  = i64;
using lulu_real = f64;

// #define LULU_UINT_MAX   (cast(lulu_uint)-1)
#define LULU_INT_MAX    INT64_MAX
#define LULU_INT_MIN    INT64_MIN

template<class T>
static inline T
max(T a, T b)
{
    return (a < b) ? b : a;
}

template<class T>
static inline void
swap(T *restrict const a, T *restrict const b)
{
    T tmp = *a;
    *a = *b;
    *b = tmp;
}

template<class T> static inline T num_neg(T a)      { return -a;    }
template<class T> static inline T num_add(T a, T b) { return a + b; }
template<class T> static inline T num_sub(T a, T b) { return a - b; }
template<class T> static inline T num_mul(T a, T b) { return a * b; }
template<class T> static inline T num_div(T a, T b) { return a / b; }
template<class T> static inline T num_mod(T a, T b) { return a % b; }

// Specialization for reals becuase C/C++ doesn't allow direct modulo.
template<>
inline lulu_real
num_mod(lulu_real a, lulu_real b)
{
    return std::floor(a / b) * b;
}

template<class T> static inline bool num_eq (T a, T b) { return a == b; }
template<class T> static inline bool num_lt (T a, T b) { return a <  b; }
template<class T> static inline bool num_leq(T a, T b) { return a <= b; }
