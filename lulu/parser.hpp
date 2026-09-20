#pragma once

#include "lulu.h"
#include "lexer.hpp"
#include "chunk.hpp"
#include "mem.hpp"
#include "expr.hpp"

// If you exceed this, you should probably rethink what you did!
#define PARSER_MAX_RECURSIONS   250

// Defined in `compiler.h`.
struct Compiler;
struct Parser {
    // Shared state.
    lulu_State *L        = nullptr;
    Compiler *  compiler = nullptr;

    // Parser state.
    Lexer lexer;
    Token token;

    // Just in case we want to allocate a message.
    Scratch *scratch = nullptr;

    // Tracked to prevent stack overflow.
    int recursions = 0;
};


struct ParserData {
    String  path, input;
    Chunk   chunk;
    Scratch scratch;
};

LULU_INTERNAL_FUNC Chunk *
parser_parse(lulu_State *L, ParserData *data);

[[noreturn]] LULU_INTERNAL_FUNC void
parser_error_at(Parser *p, char const *info, Loc const &where);

[[noreturn]] static void
parser_error(Parser *p, char const *info)
{
    parser_error_at(p, info, p->token.loc);
}

[[noreturn]] static void
parser_error_token(Parser *p, char const *info, Token const &token)
{
    parser_error_at(p, info, token.loc);
}

[[noreturn]] static void
parser_error_expr(Parser *p, char const *info, Expr const *expr)
{
    parser_error_at(p, info, expr->loc);
}

