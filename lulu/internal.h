#ifndef LULU_INTERNAL_H
#define LULU_INTERNAL_H

// standard
#include <stdbool.h>
#include <stdint.h>

// local
#include "lulu.h"

#define cast(T)         (T)
#define unused(expr)    cast(void)(expr)

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

typedef union Value_Literal Value_Literal;
union Value_Literal {
    u64 u;
    i64 i;
    f64 f;
};

#endif // !LULU_INTERNAL_H
