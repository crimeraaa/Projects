// standard
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>

#include "state.h"
#include "parser.h"
#include "type.h"

struct lulu_Error_Handler {
    lulu_Error_Handler *prev;
    volatile lulu_Error err;
    jmp_buf             env;
};

// Wrap the initialization calls that may throw.
static void
state_open_protected(lulu_State *L, void *user_data)
{
    unused(user_data);
    mem_arena_init(L, &L->arena);
    type_env_init(L, &L->types);
}

LULU_API lulu_State *
lulu_open(void)
{
    // TODO(2026-07-04): Make non-static, i.e. allocate a new instance!
    static lulu_State _L;
    lulu_State *      L;
    lulu_Error        err;

    L          = &_L;
    L->handler = nullptr;
    err        = state_try(L, state_open_protected, nullptr);
    return (!err) ? L : nullptr;
}

LULU_API void
lulu_close(lulu_State *L)
{
    type_env_destroy(L, &L->types);
    mem_arena_destroy(&L->arena);
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

static void
state_parse(lulu_State *L, void *user_data)
{
    Parser_Data *data = cast(Parser_Data *)user_data;
    Ast *        root = parser_parse(L, data);

    ast_print(root);
}

LULU_INTERNAL_FUNC lulu_Error
state_parse_protected(lulu_State *L, String path, String input)
{
    Parser_Data data = {path, input, mem_scratch_begin(&L->arena)};
    lulu_Error  err  = state_try(L, state_parse, &data);
    mem_scratch_end(&data.scratch);
    return err;
}
