#ifndef PROJECTS_COMMON_H
#define PROJECTS_COMMON_H

#include <stdalign.h>   // alignof
#include <stdbool.h>    // bool, true, false
#include <stddef.h>     // size_t

// Easier to grep.
#define cast(T)         (T)
#define unused(expr)    cast(void)(expr)

#endif // !PROJECTS_COMMON_H
