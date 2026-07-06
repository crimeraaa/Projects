#ifndef LULU_STATE_H
#define LULU_STATE_H

#include "lulu.h"
#include "internal.h"
#include "strings.h"
#include "mem.h"

typedef struct Type          Type;
typedef struct TypeEnv_Entry TypeEnv_Entry;
struct TypeEnv_Entry {
    String key;
    Type * type;
};

typedef struct TypeEnv TypeEnv;
struct TypeEnv {
    TypeEnv_Entry *data;
    usize          used;
    usize          cap;
};

typedef struct lulu_Error_Handler lulu_Error_Handler;
struct lulu_State {
    TypeEnv types;

    // Used to allocate nodes for the compilation stage.
    Arena arena;

    // Error handlers only matter for `state_{try,throw}`.
    lulu_Error_Handler *handler;
};

typedef void (*Protected_Fn)(lulu_State *L, void *user_data);

LULU_INTERNAL_FUNC lulu_Error
state_try(lulu_State *L, Protected_Fn fn, void *user_data);

LULU_INTERNAL_FUNC LULU_NORETURN void
state_throw(lulu_State *L, lulu_Error err);

LULU_INTERNAL_FUNC lulu_Error
state_parse_protected(lulu_State *L, String path, String input);

#endif // !LULU_STATE_H
