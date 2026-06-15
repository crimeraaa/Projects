#pragma once

#include "../common.h"

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

