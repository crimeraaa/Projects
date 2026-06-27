#pragma once

#include <climits> // CHAR_BIT
#include <cstddef> // std::size_t
#include <cstdint>

// Because C-style casts are atrocious
#define cast(T)         (T)
#define global          extern
#define internal        static
#define local_persist   static

#define unused(expr)    (cast(void)sizeof(expr))
#define count_of(expr)  (sizeof(expr) / sizeof(*(expr)))
#define bit_size(expr)  (sizeof(expr) * CHAR_BIT)

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;
