// standard
#include <cstdlib>
#include <cstdio>

#include "state.hpp"
#include "parser.hpp"
#include "type.hpp"
#include "debug.hpp"
#include "vm.hpp"

struct lulu_ErrorHandler {
    lulu_ErrorHandler *prev;
    lulu_Error         error;
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

LULU_INTERNAL_FUNC void
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

static void
state_parse(lulu_State *L, void *user_data)
{
    ParserData &data  = *cast(ParserData *)user_data;
    Chunk *     chunk = Parser::parse(L, data);
    debug_disassemble(chunk);
    vm_execute(L, chunk);
}

LULU_INTERNAL_FUNC lulu_Error
state_parse_protected(lulu_State *L, String path, String input)
{
    Chunk      chunk;
    ParserData data = {path, input, chunk, mem_scratch_begin(&L->arena)};
    lulu_Error err  = state_try(L, state_parse, &data);
    mem_scratch_free_all(&data.scratch);
    data.chunk.code.free(L);
    data.chunk.constants.free(L);
    return err;
}
