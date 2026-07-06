#ifndef LULU_PARSER_H
#define LULU_PARSER_H

#include "lulu.h"
#include "lexer.h"
#include "mem.h"
#include "ast.h"

// If you exceed this, you should probably rethink what you did!
#define PARSER_MAX_RECURSIONS   250

typedef struct Parser Parser;
struct Parser {
    lulu_State *L;
    Lexer lexer;
    Token token;
    Token consumed;

    // Just in case we want to allocate a message.
    Scratch *scratch;

    // Tracked to prevent stack overflow.
    int recursions;
};


typedef struct Parser_Data Parser_Data;
struct Parser_Data {
    String  path, input;
    Scratch scratch;
};

LULU_INTERNAL_FUNC Ast *
parser_parse(lulu_State *L, Parser_Data *data);

#endif // !LULU_PARSER_H
