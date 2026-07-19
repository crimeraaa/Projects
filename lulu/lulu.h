#ifndef LULU_H
#define LULU_H

#include <stddef.h> /* size_t */

/*
 LULU_API:
    This marks function declarations that are visible to outside
    libraries. That is, they can be referred to outside of Lulu.

    Note that Lulu declares no externally visible variables.

 LULU_INTERNAL_FUNC:
    This marks functions declarations that are visible only to other
    translation units within Lulu. They should not (ideally) become
    visible to external libraries.

    Note that we do unity builds by default.

 LULU_INTERNAL_DATA:
    This marks global data variables that are visible only to other
    translation units within Lulu. They should not (ideally) become
    visible to external libraries.

    Note that we do unity builds by default.
 */
#ifdef __cplusplus
#define LULU_API            extern "C"
#define LULU_INTERNAL_FUNC  extern "C"
#define LULU_INTERNAL_DATA  extern
#else
#define LULU_API    extern
#define LULU_INTERNAL_FUNC  extern
#define LULU_INTERNAL_DATA  extern
#endif

/*
 Description:
    This is the default alignment used by the internal allocation API.
    This ensures that all allocations are aligned to this boundary.
    Such a guarantee may be useful to, say, ensure that we can always
    read/write the target architecture's word size at time such as in
    `memcpy`.
 */
#define LULU_DEFAULT_ALIGN  sizeof(void *)

typedef enum lulu_Error {
    LULU_OK, /* No error occured. */
    LULU_SYNTAX_ERROR,
    LULU_RUNTIME_ERROR,
    LULU_MEMORY_ERROR
} lulu_Error;

typedef struct lulu_State lulu_State;

LULU_API lulu_State *
lulu_open(void);

LULU_API void
lulu_close(lulu_State *L);

LULU_API const char *
lulu_error_string(lulu_Error err);

LULU_API lulu_Error
lulu_compile(lulu_State *L, char const *path, char const *input, size_t len);

#endif // !LULU_H

