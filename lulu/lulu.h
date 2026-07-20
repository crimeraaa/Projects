#ifndef LULU_H
#define LULU_H

#include <stddef.h> /* size_t          */
#include <stdint.h> /* intN_t, uintN_t */
#include <limits.h> /* INTn_MAX, UINTn_MAX */

#define LULU_INT_TYPE   int64_t
#define LULU_INT_MAX    INT64_MAX
#define LULU_INT_MIN    INT64_MIN

#define LULU_REAL_TYPE  double

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

typedef LULU_INT_TYPE    lulu_int;
typedef LULU_REAL_TYPE   lulu_real;
typedef union lulu_Value lulu_Value;

union lulu_Value {
    /* Two-fold (2-fold) job: actual (signed) integers, and booleans. This
       allows us to implement boolean operations in terms of integer ones. */
    lulu_int  i;
    lulu_real r;
    void *    p;
};

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

