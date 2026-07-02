#ifndef LULU_INTERNAL_H
#define LULU_INTERNAL_H

// standard
#include <stdbool.h>
#include <stdint.h>

// local
#include "lulu.h"

#if defined(__GNUC__) || defined(__clang__)

#define LULU_NORETURN       __attribute__((__noreturn__))
#define LULU__ASSERT_IMPL() __builtin_trap()

#elif defined(_MSC_VER) // ^^^ GCC, clang; vvv MSVC

#define LULU_NORETURN       __declspec(noreturn)
#define LULU__ASSERT_IMPL() __debugbreak()

#else // ^^^ MSVC ; vvv <unknown>

#include <stdlib.h>

// No harm in not knowing, but your compiler can't optimize for such cases.
#define LULU_NORETURN
#define LULU__ASSERT_IMPL() abort()
#endif

#define cast(T)         (T)
#define unused(expr)    cast(void)(expr)

#if 1
#define LULU_LOGFLN(fmt, ...) \
    fprintf(stderr, "%s:%i: " fmt "\n", __FUNCTION__, __LINE__, __VA_ARGS__)
#else
#define LULU_LOGFLN(fmt, ...)
#endif
#define LULU_LOGLN(msg) LULU_LOGFLN("%s", msg)

#define LULU_USE_ASSERT 1
#if LULU_USE_ASSERT
#define LULU_ASSERTF(expr, fmt, ...)                                           \
do {                                                                           \
    if (!cast(bool)(expr)) {                                                   \
        LULU_LOGFLN(fmt, __VA_ARGS__);                                         \
        LULU__ASSERT_IMPL();                                                   \
    }                                                                          \
} while (0)
#else
#define LULU_ASSERTF(expr, fmt, ...)
#endif // LULU_USE_ASSERT

#define LULU_ASSERTLN(e, msg)   LULU_ASSERTF(e, "%s", msg)
#define LULU_ASSERT(e)          LULU_ASSERTLN(e, "Assertion '" #e "'failed!")

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float   f32;
typedef double  f64;

typedef size_t    usize;
typedef uintptr_t uintptr;

#endif // !LULU_INTERNAL_H
