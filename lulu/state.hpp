#pragma once

#include "lulu.h"
#include "internal.hpp"
#include "strings.hpp"
#include "mem.hpp"
#include "type.hpp"

struct lulu_Error_Handler;
struct lulu_State {
    TypeEnv types;

    // Used to allocate nodes for the compilation stage.
    Arena arena;

    // Error handlers only matter for `state_{try,throw}`.
    lulu_Error_Handler *handler;
};

using Protected_Fn = void (*)(lulu_State *L, void *user_data);

LULU_INTERNAL_FUNC lulu_Error
state_try(lulu_State *L, Protected_Fn fn, void *user_data);

LULU_INTERNAL_FUNC LULU_NORETURN void
state_throw(lulu_State *L, lulu_Error err);

LULU_INTERNAL_FUNC lulu_Error
state_parse_protected(lulu_State *L, String path, String input);

