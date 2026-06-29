#include "state.h"

struct lulu_Error_Handler {
    lulu_Error_Handler *prev;
    volatile lulu_Error err;
    jmp_buf             env;
};


LULU_API lulu_State *
lulu_open(void)
{
    static lulu_State L = {/*handler=*/NULL};
    return &L;
}

LULU_INTERNAL_FUNC lulu_Error
state_try(lulu_State *L, Protected_Fn fn, void *user_data)
{
    lulu_Error_Handler handler = {L->handler, LULU_OK};
    L->handler = &handler;
    if (setjmp(handler.env) == 0) {
        fn(L, user_data);
    }

    L->handler = handler.prev;
    return handler.err;
}

LULU_INTERNAL_FUNC void
state_throw(lulu_State *L, lulu_Error err)
{
    L->handler->err = err;
    longjmp(L->handler->env, 1);
}
