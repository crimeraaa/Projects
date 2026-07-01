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
    AST_DECL,
    AST_ASSIGN,
    AST_UNARY,
    AST_BINARY,
};

typedef enum   Ast_Kind    Ast_Kind;
typedef union  Ast         Ast;
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
    Ast      *expr;
};

typedef struct Ast_Decl Ast_Decl;
struct Ast_Decl {
    Ast_Kind  kind;
    Ast      *name;  // Expression before '='.
    Ast      *type;  // Expression after ':' but before '='.
    Ast      *value; // Expression after '=' to assign `name` with.
};

typedef struct Ast_Assign Ast_Assign;
struct Ast_Assign {
    Ast_Kind kind;
    Token    op;   // The '=' operator.
    Ast     *left;
    Ast     *right;
};

typedef struct Ast_Unary Ast_Unary;
struct Ast_Unary {
    Ast_Kind  kind;
    Token     op;
    Ast      *arg;
};

typedef struct Ast_Binary Ast_Binary;
struct Ast_Binary {
    Ast_Kind  kind;
    Token     op;
    Ast      *left;
    Ast      *right;
};

union Ast {
    Ast_Kind    kind;
    Ast_Literal literal;
    Ast_Ident   ident;
    Ast_Decl    decl;
    Ast_Assign  assign;
    Ast_Paren   paren;
    Ast_Unary   unary;
    Ast_Binary  binary;
};

LULU_INTERNAL_FUNC Ast *
ast_literal_new(lulu_State *L, const Token *token);

LULU_INTERNAL_FUNC Ast *
ast_ident_new(lulu_State *L, const Token *token);

LULU_INTERNAL_FUNC Ast *
ast_paren_new(lulu_State *L, const Token *open, const Token *close, Ast *expr);

LULU_INTERNAL_FUNC Ast *
ast_unary_new(lulu_State *L, const Token *op, Ast *arg);

LULU_INTERNAL_FUNC Ast *
ast_decl_new(lulu_State *L, Ast *name, Ast *type, Ast *value);

LULU_INTERNAL_FUNC Ast *
ast_assign_new(lulu_State *L, const Token *op, Ast *left, Ast *right);

LULU_INTERNAL_FUNC Ast *
ast_binary_new(lulu_State *L, const Token *op, Ast *left, Ast *right);

LULU_INTERNAL_FUNC void
ast_print(const Ast *a);

#endif // !LULU_AST_H
