#include "lulu.h"

#include "ast.c"
#include "arena.c"
#include "state.c"
#include "lexer.c"
#include "parser.c"
#include "value.c"

LULU_API const char *
lulu_error_string(lulu_Error err)
{
    switch (err) {
    case LULU_OK:            return "No error";
    case LULU_SYNTAX_ERROR:  return "Syntax error";
    case LULU_RUNTIME_ERROR: return "Runtime error";
    case LULU_MEMORY_ERROR:  return "Out of memory";
    }
    return NULL;
}

typedef struct Pdata Pdata;
struct Pdata {
    String path, input;
};

static void
state_parse_protected(lulu_State *L, void *user_data)
{
    Pdata *pd;
    Ast *  prog;

    pd   = cast(Pdata *)user_data;
    prog = parser_parse(L, pd->path, pd->input);
    ast_print(prog);
}

LULU_API lulu_Error
lulu_compile(lulu_State *L, const char *path, const char *input, size_t len)
{
    Pdata pd;
    lulu_Error err;
    pd.path  = string_make_cstring(path);
    pd.input = string_make(input, len);
    err      = state_try(L, state_parse_protected, &pd);

    // Don't call this inside the parse proper so that in case of errors
    // we still free the whole program.
    arena_free_all(&L->arena);
    return err;
}
