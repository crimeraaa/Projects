#ifndef LULU_H
#define LULU_H

#include <stddef.h> /* size_t */

/*
 Description:
    This marks function declarations that are visible to outside
    libraries. That is, they can be referred to outside of Lulu.

    Note that Lulu declares no externally visible variables.
 */
#define LULU_API            extern

/*
 Description:
    This marks functions declarations that are visible only to other
    translation units within Lulu. They should not (ideally) become
    visible to external libraries.

    Note that we do unity builds by default.
 */
#define LULU_INTERNAL_FUNC  extern

/*
 Description:
    This marks global data variables that are visible only to other
    translation units within Lulu. They should not (ideally) become
    visible to external libraries.

    Note that we do unity builds by default.
 */
#define LULU_INTERNAL_DATA  extern

/*
 Description:
    This is the default alignment used by the internal allocation API.
    This ensures that all allocations are aligned to this boundary.
    Such a guarantee may be useful to, say, ensure that we can always
    read/write the target architecture's word size at time such as in
    `memcpy`.
 */
#define LULU_DEFAULT_ALIGN  sizeof(void *)

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

