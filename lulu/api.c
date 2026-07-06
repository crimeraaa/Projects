#include "lulu.h"

#include "ast.c"
#include "state.c"
#include "lexer.c"
#include "mem.c"
#include "parser.c"
#include "strings.h"
#include "type.c"
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
    LULU_UNREACHABLE();
    return nullptr;
}

LULU_API lulu_Error
lulu_compile(lulu_State *L, const char *path, const char *input, size_t len)
{
    String path2  = string_make_cstring(path);
    String input2 = string_make(input, len);
    return state_parse_protected(L, path2, input2);
}
