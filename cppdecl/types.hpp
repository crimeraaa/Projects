#pragma once

#include <cstdint>

using  u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using f64 = double;

enum Error : u8 {
    ERR_NONE,

    // Not necessarily an error!
    ERR_EOF,

    // Lexing and parsing errors.
    ERR_UNEXPECTED_TOKEN,
    ERR_UNTERMINATED_COMMENT,
    ERR_UNTERMINATED_STRING,
    ERR_INVALID_NUMBER,

    // Allocation errors.
    ERR_OUT_OF_MEMORY,
    ERR_INVALID_ALLOCATOR,
};

