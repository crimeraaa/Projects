#ifndef LULU_PARSER_H
#define LULU_PARSER_H

// standard
#include <setjmp.h>

// local
#include "lexer.h"
#include "ast.h"

typedef struct Parser Parser;
struct Parser {
    lulu_State *L;
    Lexer lexer;
    Token token;
    Token consumed;
};

LULU_INTERNAL_FUNC Ast *
parser_parse(lulu_State *L, String path, String input);

#endif // !LULU_PARSER_H
