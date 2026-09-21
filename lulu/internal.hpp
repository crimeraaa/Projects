#pragma once

// standard
#include <cstddef>   // size_t
#include <cstdint>   //  u?int\d+_t
#include <cmath>     // floor

#include "lulu.h"

#if defined(__GNUC__) || defined(__clang__)
#   define LULU_UNREACHABLE()  __builtin_trap()
#   define LULU__ASSERT_IMPL() __builtin_trap()
#   define restrict            __restrict__
#elif defined(_MSC_VER) // ^^^ GCC, clang; vvv MSVC
#   define LULU__ASSERT_IMPL() __debugbreak()
#   define LULU_UNREACHABLE()  cast(void)0
#   define LULU_FORMAT(f, a)
#   define restrict            __restrict
#else // ^^^ MSVC ; vvv <unknown>
#   include <cstdlib>
#   define LULU_FORMAT(f, a)
#   define LULU_UNREACHABLE()  cast(void)0
#   define restrict

// Always works but may not be good for debuggers.
#   define LULU__ASSERT_IMPL() std::abort()
#endif

#if defined(__has_feature)
#   if __has_feature(address_sanitizer) && !defined(__SANITIZE_ADDRESS__)
#       define __SANITIZE_ADDRESS__
#   endif
#endif

#ifdef __SANITIZE_ADDRESS__
#   pragma message("Using intentional segault for assertion failures!")
#   undef LULU__ASSERT_IMPL
#   define LULU__ASSERT_IMPL() cast(void)(*(volatile int *)0 = 67)
#endif

#define cast(T)         (T)
#define unused(expr)    cast(void)(expr)
#define count_of(expr)  (sizeof(expr) / sizeof((expr)[0]))

#if 1
#   include <cstdio> // fprintf
#   define LULU_LOGF(fmt, ...) \
    std::fprintf(stderr, "%s:%i: " fmt "\n", __func__, __LINE__, __VA_ARGS__)
#else
#   define LULU_LOGF(fmt, ...)   cast(void)0
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
#   define LULU_ASSERTF(expr, fmt, ...)                                        \
    (cast(bool)(expr)                                                          \
        ? cast(void)0                                                          \
        : (LULU_LOGF(fmt, __VA_ARGS__), LULU__ASSERT_IMPL()))
#else // ^^^ LULU_USE_ASSERT | vvv LULU_USE_ASSERT
#   define LULU_ASSERTF(expr, fmt, ...)    cast(void)0
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

// Stupid typedef because we can't redefine `int`.
using intr = lulu_int;
using real = lulu_real;

// #define LULU_UINT_MAX   (cast(lulu_uint)-1)
#define LULU_INT_MAX    INT64_MAX
#define LULU_INT_MIN    INT64_MIN

template<class T>
static inline T
max(T a, T b) { return (a > b) ? a : b; }

template<class T>
static inline T
min(T a, T b) { return (a < b) ? a : b; }

template<class T>
static inline void
swap(T *restrict const a, T *restrict const b)
{
    T tmp = *a;
    *a = *b;
    *b = tmp;
}

template<class T>
static inline T
num_bnot(T a)      { return ~a;    }

template<class T>
static inline T
num_band(T a, T b) { return a & b; }

template<class T>
static inline T
num_bor (T a, T b) { return a | b; }

template<class T>
static inline T
num_bxor(T a, T b) { return a ^ b; }

template<class T>
static inline T
num_neg (T a)      { return -a;    }

template<class T>
static inline T
num_add (T a, T b) { return a + b; }

template<class T>
static inline T
num_sub (T a, T b) { return a - b; }

template<class T>
static inline T
num_mul (T a, T b) { return a * b; }

template<class T>
static inline T
num_div (T a, T b) { return a / b; }

template<class T>
static inline T
num_mod (T a, T b) { return a % b; }

// Specialization for reals becuase C/C++ doesn't allow direct modulo.
template<>
inline real
num_mod(real a, real b) { return std::floor(a / b) * b; }

template<class T>
static inline bool
num_eq(T a, T b) { return a == b; }

template<class T>
static inline bool
num_lt(T a, T b) { return a <  b; }

template<class T>
static inline bool
num_leq(T a, T b) { return a <= b; }

template<class T>
struct Some {
    T value;

    explicit constexpr
    Some(T value) : value{value} {}
};

struct None {};

template<class T>
class Option {
    bool tag;
    T    value;

public:
    using Some = Some<T>;
    using None = None;

    Option(Some some) : tag{true}, value{some.value} {}
    Option(None)      : tag{false}                   {}

    /// Check whether we are currently of the `Some` variant or not.
    bool
    is_some()        const noexcept { return this->tag; }

    /// Checks wheter we are currently of the `Some` variant and that the
    /// value thereof satisfies the given predicate callback function.
    template<class F>
    bool
    is_some_and(F f) const noexcept { return this->is_some() && f(this->value); }

    /// Checks whether we are currently of the `None` variant or not.
    bool
    is_none()        const noexcept { return !this->is_some(); }

    /// Checks whether we are currently of the `None` variant, or that the
    /// `Some` variant's value satisfies the given predicate callback function.
    template<class F>
    bool
    is_none_or(F f)  const noexcept { return this->is_none() || f(this->value); }

    /// Passes the value of the `Some` variant through the given mapping function
    /// or returns the fallback value.
    template<class U, class F>
    U
    map_or_else(F f, U fallback) const noexcept
    {
        return this->is_some() ? f(this->value) : fallback;
    }

    /// Retrives the value of the `Some` variant. It is undefined behavior
    /// to call this when we are actually of the `None` variant.
    T
    unwrap()
    {
        LULU_ASSERT(this->is_some());
        return this->value;
    }

    /// Retrieves the value of the `Some` variant or uses the fallback value.
    T
    unwrap_or(T fallback) noexcept
    {
        return this->is_some() ? this->value : fallback;
    }
}; // class Option

template<class T>
struct Ok {
    T value;

    explicit constexpr
    Ok(T value) : value{value} {}
};

template<class E>
struct Err {
    E error;

    explicit constexpr
    Err(E error) : error{error} {}
};

template<class T, class E>
class Result {

    bool tag;
    union {
        T value;
        E error;
    };

public:
    using Ok  = Ok<T>;
    using Err = Err<E>;

    Result(Ok  ok)  : tag{true},  value{ok.value}  {}
    Result(Err err) : tag{false}, error{err.error} {}

    /// Control flow that maps some `Result<T, E>` to a new `Result<U, E>`.
    ///
    /// This uses the result of `f()` on the `Ok` variant's value, or propagates
    /// the `Err` variant into the new result type. The mapping function must
    /// return some `Result<U, E>`. This is because the mapping function can
    /// also return another `Err` variant during its processing.
    template<class U, class F>
    Result<U, E>
    and_then(F f) const noexcept
    {
        if (this->is_ok()){
            return f(this->value);
        } else {
            return Err(this->error);
        }
    }

    /// Checks whether we are currently of the `Ok` variant.
    bool
    is_ok () const noexcept { return this->tag;      }

    /// Checks whether we are currently of the `Err` variant.
    bool
    is_err() const noexcept { return !this->is_ok(); }

    /// Converts ourselves to an `Option<T>`. Specifically, the `Ok` variant
    /// is wrapped in an `Option<T>::Some` and the `Err` variant is wrapped in
    /// an `Option<T>::None`.
    Option<T>
    ok() noexcept
    {
        if (this->is_ok()) {
            return Some(this->value);
        } else {
            return None{};
        }
    }

    /// Retrives the value of the `Ok` variant. It is undefined behavior to
    /// call this when we are actually of the `Err` variant.
    T
    unwrap()
    {
        LULU_ASSERT(this->is_ok());
        return this->value;
    }

    /// Retrieves the value of the `Err` variant. It is undefined behavior to
    /// call this when we are actually of the `Ok` variant.
    E
    unwrap_err()
    {
        LULU_ASSERT(this->is_err());
        return this->error;
    }
};
