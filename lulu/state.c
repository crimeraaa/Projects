// standard
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>

#include "state.h"

struct lulu_Error_Handler {
    lulu_Error_Handler *prev;
    volatile lulu_Error err;
    jmp_buf             env;
};

LULU_API lulu_State *
lulu_open(void)
{
    // TODO(2026-07-04): Make non-static, i.e. allocate a new instance!
    static lulu_State L;
    if (!arena_init(&L.arena)) {
        return NULL;
    }
    L.handler = NULL;
    return &L;
}

LULU_API void
lulu_close(lulu_State *L)
{
    arena_destroy(&L->arena);
}

LULU_INTERNAL_FUNC lulu_Error
state_try(lulu_State *L, Protected_Fn fn, void *user_data)
{
    lulu_Error_Handler handler;

    // Push new error handler.
    handler.prev = L->handler;
    handler.err  = LULU_OK;
    L->handler   = &handler;

    // Run the protected call. The first thrown error will go back here.
    if (setjmp(handler.env) == 0) {
        fn(L, user_data);
    }

    // Pop the error handler.
    L->handler = handler.prev;
    return handler.err;
}

LULU_INTERNAL_FUNC void
state_throw(lulu_State *L, lulu_Error err)
{
    if (L->handler) {
        L->handler->err = err;
        longjmp(L->handler->env, 1);
    } else {
        const char *msg = lulu_error_string(err);
        fprintf(stderr, "[FATAL] Unprotected call to Lulu API (%s)\n", msg);
        exit(1);
    }
}

