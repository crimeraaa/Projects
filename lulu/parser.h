#ifndef LULU_PARSER_H
#define LULU_PARSER_H

#include "lulu.h"
#include "lexer.h"
#include "ast.h"

// If you exceed this, you should probably rethink what you did!
#define PARSER_MAX_RECURSIONS   250

typedef struct Parser Parser;
struct Parser {
    lulu_State *L;
    Lexer lexer;
    Token token;
    Token consumed;

    int recursions;
};

LULU_INTERNAL_FUNC Ast *
parser_parse(lulu_State *L, String path, String input);

#endif // !LULU_PARSER_H
