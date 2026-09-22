#pragma once

// standard
#include <cstdlib>
#include <cstdio>

#include "lulu.h"
#include "strings.cpp"
#include "mem.cpp"
#include "type.cpp"
#include "parser.cpp"
#include "debug.cpp"
#include "vm.cpp"

struct lulu_ErrorHandler {
    lulu_ErrorHandler *prev;
    lulu_Error         error;
};

struct lulu_State {
    TypeEnv types;

    // Used to allocate nodes for the compilation stage.
    Arena arena;

    // Error handlers only matter for `state_{try,throw}`.
    lulu_ErrorHandler *handler;
};

using Protected_Fn = void (*)(lulu_State *L, void *user_data);

TypeEnv &
state_type_env(lulu_State *L)
{
    return L->types;
}

lulu_Error
state_try(lulu_State *L, Protected_Fn fn, void *user_data)
{
    // Push new error handler.
    lulu_ErrorHandler handler{L->handler, LULU_OK};
    L->handler = &handler;

    // Run the protected call. The first thrown error will go back here.
    try {
        fn(L, user_data);
    } catch (lulu_Error error) {
        handler.error = error;
    }

    // Pop the error handler.
    L->handler = handler.prev;
    return handler.error;
}

void
state_throw(lulu_State *L, lulu_Error err)
{
    if (L->handler) {
        throw err;
    } else {
        char const *msg = lulu_error_string(err);
        std::fprintf(stderr, "[FATAL] Unprotected call to Lulu API (%s)\n", msg);
        std::exit(1);
    }
}


// Wrap the initialization calls that may throw.
static void
state_open_protected(lulu_State *L, void *user_data)
{
    unused(user_data);
    L->arena.init(L);
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
    L->arena.destroy();
}

static void
state_parse(lulu_State *L, void *user_data)
{
    ParserData &data  = *cast(ParserData *)user_data;
    Chunk *     chunk = Parser::parse(L, data);
    debug_disassemble(chunk);
    vm_execute(L, chunk);
}

lulu_Error
state_parse_protected(lulu_State *L, String path, String input)
{
    Chunk      chunk;
    ParserData data = {path, input, chunk, Scratch::make(&L->arena)};
    lulu_Error err  = state_try(L, state_parse, &data);
    data.scratch.destroy();
    data.chunk.code.free(L);
    data.chunk.constants.free(L);
    return err;
}
