#pragma once

#include "lulu.h"
#include "lexer.hpp"
#include "chunk.hpp"
#include "mem.hpp"

#define PARSER_CONSTANT_FOLDING 1

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


typedef struct ParserData ParserData;
struct ParserData {
    String  path, input;
    Chunk   chunk;
    Scratch scratch;
};

LULU_INTERNAL_FUNC Chunk *
parser_parse(lulu_State *L, ParserData *data);

LULU_INTERNAL_FUNC [[noreturn]] void
parser_error_at(Parser *p, char const *info, Token const &t);

