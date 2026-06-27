#ifndef LULU_H
#define LULU_H

#include "../common.h"

enum lulu_Error {
    // No error occured!
    LULU_OK,

    // Not necessarily an error.
    LULU_EOF,

    // Lexer errors.
    LULU_UNEXPECTED_CHARACTER,
    LULU_INVALID_NUMBER,
    LULU_UNTERMINATED_STRING,

    // Parser errors.
    LULU_UNEXPECTED_TOKEN,

    // Runtime errors.
    LULU_RUNTIME_ERROR,
};

typedef enum lulu_Error lulu_Error;

#endif // !LULU_H

