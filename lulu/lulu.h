#ifndef LULU_H
#define LULU_H

#include <stddef.h> /* size_t */

/* Functions that can be exported to other libraries. */
#define LULU_API           extern

/* Functions that are not to be exported to other libraries.
 * Note that we do unity builds by default. */
#define LULU_INTERNAL_FUNC extern

/* Data variables that are not to be exported to other libraries.*
 * Note that we do unity builds by default. */
#define LULU_INTERNAL_DATA extern

enum lulu_Error {
    LULU_OK, /* No error occured. */
    LULU_SYNTAX_ERROR,
    LULU_RUNTIME_ERROR,
    LULU_MEMORY_ERROR
};

typedef enum   lulu_Error lulu_Error;
typedef struct lulu_State lulu_State;

LULU_API lulu_State *
lulu_open(void *backing_buf, size_t backing_size);

LULU_API const char *
lulu_error_string(lulu_Error err);

LULU_API lulu_Error
lulu_compile(lulu_State *L, const char *path, const char *input, size_t len);

#endif // !LULU_H

