#ifndef PROJECTS_COMMON_H
#define PROJECTS_COMMON_H

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

#endif // !PROJECTS_COMMON_H
