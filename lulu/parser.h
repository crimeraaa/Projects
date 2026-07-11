#ifndef LULU_PARSER_H
#define LULU_PARSER_H

#include "lulu.h"
#include "lexer.h"
#include "chunk.h"
#include "mem.h"
#include "value.h"
#include "type.h"

#define PARSER_CONSTANT_FOLDING 0

// If you exceed this, you should probably rethink what you did!
#define PARSER_MAX_RECURSIONS   250

// Defined in `compiler.h`.
typedef struct Compiler Compiler;
typedef struct Parser   Parser;
struct Parser {
    // Shared state.
    lulu_State *L;
    Compiler *  compiler;

    // Parser state.
    Lexer lexer;
    Token token;

    // Just in case we want to allocate a message.
    Scratch *scratch;

    // Tracked to prevent stack overflow.
    int recursions;
};


typedef struct ParserData ParserData;
struct ParserData {
    String  path, input;
    Chunk   chunk;
    Scratch scratch;
};

LULU_INTERNAL_FUNC Chunk *
parser_parse(lulu_State *L, ParserData *data);

// The most common case is to error at the current token.
LULU_INTERNAL_FUNC LULU_NORETURN void
parser_error(Parser *p, const char *info);

LULU_INTERNAL_FUNC LULU_NORETURN void
parser_error_at(Parser *p, const char *info, const Token *t);

#endif // !LULU_PARSER_H
