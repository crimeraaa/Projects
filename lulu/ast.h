#ifndef LULU_AST_H
#define LULU_AST_H

#include "internal.h"
#include "arena.h"
#include "lexer.h"
#include "value.h"

enum Ast_Kind {
    AST_NONE,
    AST_LITERAL,
    AST_IDENT, // Don't know yet if type name or variable.
    AST_PAREN, // Need to differntiate casts.
    AST_UNARY,
    AST_BINARY,
};

typedef enum   Ast_Kind    Ast_Kind;
typedef union  Ast_Node    Ast_Node;
typedef struct Ast_Literal Ast_Literal;
struct Ast_Literal {
    Ast_Kind      kind;
    Token         token;
    Value_Literal value;
};

typedef struct Ast_Ident Ast_Ident;
struct Ast_Ident {
    Ast_Kind kind;
    u32      hash;
    Token    token;
};

typedef struct Ast_Paren Ast_Paren;
struct Ast_Paren {
    Ast_Kind  kind;
    Token     open;
    Token     close;
    Ast_Node *expr;
};

typedef struct Ast_Unary Ast_Unary;
struct Ast_Unary {
    Ast_Kind  kind;
    Token     op;
    Ast_Node *arg;
};

typedef struct Ast_Binary Ast_Binary;
struct Ast_Binary {
    Ast_Kind  kind;
    Token     op;
    Ast_Node *left;
    Ast_Node *right;
};

union Ast_Node {
    Ast_Kind    kind;
    Ast_Literal literal;
    Ast_Ident   ident;
    Ast_Paren   paren;
    Ast_Unary   unary;
    Ast_Binary  binary;
};

LULU_INTERNAL_FUNC Ast_Node *
ast_literal_new(lulu_State *L, const Token *token);

LULU_INTERNAL_FUNC Ast_Node *
ast_ident_new(lulu_State *L, const Token *token);

LULU_INTERNAL_FUNC Ast_Node *
ast_paren_new(lulu_State *L,
    const Token *open,
    const Token *close,
    Ast_Node    *expr);

LULU_INTERNAL_FUNC Ast_Node *
ast_unary_new(lulu_State *L, const Token *op, Ast_Node *arg);

LULU_INTERNAL_FUNC Ast_Node *
ast_binary_new(lulu_State *L,
    const Token *op,
    Ast_Node    *left,
    Ast_Node    *right);

LULU_INTERNAL_FUNC void
ast_print(const Ast_Node *n);

#endif // !LULU_AST_H
