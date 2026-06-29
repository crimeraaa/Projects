#ifndef LULU_H
#define LULU_H

#include <stddef.h> /* size_t */

/* Functions that can be exported to other libraries. */
#define LULU_API           extern

/* Functions that are not to be exported to other libraries. */
#define LULU_INTERNAL_FUNC extern

/* Data variables that are not to be exported to other libraries.*/
#define LULU_INTERNAL_DATA extern

enum lulu_Error {
    LULU_OK, /* No error occured. */
    LULU_SYNTAX_ERROR,
    LULU_RUNTIME_ERROR,
    LULU_MEMORY_ERROR
};

typedef enum lulu_Error lulu_Error;
typedef struct lulu_State lulu_State;

LULU_API lulu_State *
lulu_open(void);

LULU_API const char *
lulu_error_string(lulu_Error err);

#endif // !LULU_H

