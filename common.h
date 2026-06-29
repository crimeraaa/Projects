#ifndef PROJECTS_COMMON_H
#define PROJECTS_COMMON_H

#define _DEBUG
#if defined(_DEBUG) && !defined(NDEBUG)
#include <assert.h>
#else
#define assert(expr)
#endif // _DEBUG && !NDEBUG

#include <limits.h>     // CHAR_BIT
#include <stddef.h>     // size_t
#include <stdint.h>     // [u]int[\d]+_t

// `alignof` is a keyword starting in C++11.
// `bool` is always a keyword in C++.
#ifndef __cplusplus
#include <stdalign.h>   // alignof
#include <stdbool.h>    // bool, true, false
#endif

// Easier to grep.
#define cast(T)         (T)
#define unused(expr)    cast(void)(sizeof(expr))

#define bit_size(T)     (sizeof(T) * CHAR_BIT)
#define count_of(expr)  (sizeof(expr) / sizeof((expr)[0]))

// Functions and variables that are visible globally across multiple
// translation units.
#define global          extern

// Functions and semi-global variables that are visible only to the current
// translation unit. They CANNOT be referenced outside of this.
#define internal        static

// Function-local variables that persist across calls.
// May not be thread-safe.
#define local_persist   static

typedef unsigned int       uint;
typedef unsigned long      ulong;
typedef unsigned long long ullong;
typedef long long          llong;

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;

typedef int8_t      i8;
typedef int16_t     i16;
typedef int32_t     i32;
typedef int64_t     i64;

typedef float       f32;
typedef double      f64;

#endif // !PROJECTS_COMMON_H
