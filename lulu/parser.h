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
    Token consumed, current;
    Arena *arena;
};

enum Precedence {
    PREC_NONE,
    PREC_ADD,   // x {+,-} y
    PREC_MUL,   // x {*,/,%} y
    PREC_POW,   // x ^ y
    PREC_UNARY, // {-,#,not} x
    PREC_CALL,
};

typedef enum Precedence Precedence;
typedef struct Rule Rule;
struct Rule {
    u8 left_prec, right_prec;
    u8 ast_kind;
};

LULU_INTERNAL_FUNC Ast_Node *
parser_parse(lulu_State *L,
    const char *name,
    const char *input, size_t len,
    Arena      *arena);

#endif // !LULU_PARSER_H
