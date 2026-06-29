#ifndef LULU_STATE_H
#define LULU_STATE_H

// standard
#include <setjmp.h>

// local
#include "internal.h"

typedef struct lulu_Error_Handler lulu_Error_Handler;
struct lulu_State {
    lulu_Error_Handler *handler;
};

typedef void (*Protected_Fn)(lulu_State *L, void *user_data);

LULU_INTERNAL_FUNC lulu_Error
state_try(lulu_State *L, Protected_Fn fn, void *user_data);

LULU_INTERNAL_FUNC void
state_throw(lulu_State *L, lulu_Error err);

#endif // !LULU_STATE_H
