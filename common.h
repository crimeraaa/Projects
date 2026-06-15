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

// `alignof` is a keyword starting in C++11.
// `bool` is always a keyword in C++.
#ifndef __cplusplus
#include <stdalign.h>   // alignof
#include <stdbool.h>    // bool, true, false
#endif

// Easier to grep.
#define cast(T)         (T)
#define unused(expr)    cast(void)(expr)

#define bit_size(T)     (sizeof(T) * CHAR_BIT)
#define count_of(expr)  (sizeof(expr) / sizeof((expr)[0]))

typedef unsigned int  uint;
typedef unsigned long ulong;

// Globally visible function and variable declarations.
// That is, they can be referenced across translation units.
#define global          extern

// Functions and global variables visible only to the current translation unit.
// That is, they CANNOT be referenced across translation units.
#define internal        static

// Function-local variables that persist across calls.
// May not be thread-safe.
#define local_persist   static

#endif // !PROJECTS_COMMON_H
