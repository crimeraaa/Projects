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
    Pdata    *pdata;
    Ast_Node *program;

    pdata   = cast(Pdata *)user_data;
    program = parser_parse(L, pdata->path, pdata->input);
    ast_print(program);

    // Don't call this inside the parse proper because we may recursively
    // call it in the future. So we don't want to invalidate the recursive
    // parent functions.
    arena_free_all(&L->arena);
}

LULU_API lulu_Error
lulu_compile(lulu_State *L, const char *path, const char *input, size_t len)
{
    Pdata pdata;
    pdata.path  = string_make_cstring(path);
    pdata.input = string_make(input, len);
    return state_try(L, state_parse_protected, &pdata);
}
