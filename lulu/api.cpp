#include "lulu.h"
#include "strings.hpp"

#include "opcode.cpp"
#include "mem.cpp"
#include "state.cpp"
#include "type.cpp"
#include "lexer.cpp"
#include "parser.cpp"
#include "compiler.cpp"
#include "value.cpp"
#include "debug.cpp"
#include "vm.cpp"

LULU_API char const *
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
lulu_compile(lulu_State *L, char const *path, char const *input, size_t len)
{
    return state_parse_protected(L, string_make_cstring(path), {input, len});
}
