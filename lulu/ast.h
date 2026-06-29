#ifndef LULU_AST_H
#define LULU_AST_H

#include "internal.h"
#include "arena.h"

enum Ast_Kind {
    AST_NONE,
    AST_LITERAL,
    AST_UNARY,
    AST_BINARY,
};

enum Ast_Literal_Kind {
    AST_UINT = 1,
    AST_INT,
    AST_FLOAT,
};

enum Ast_Unary_Kind {
    AST_NEG = 1,
};

enum Ast_Binary_Kind {
    AST_ADD = 1,
    AST_SUB,
    AST_MUL,
    AST_DIV,
    AST_MOD,
    AST_POW,
};

typedef enum  Ast_Kind Ast_Kind;
typedef union Ast_Node Ast_Node;

typedef enum   Ast_Literal_Kind Ast_Literal_Kind;
typedef struct Ast_Literal      Ast_Literal;
struct Ast_Literal {
    Ast_Kind         node_kind;
    Ast_Literal_Kind literal_kind;
    Value_Literal    value;
};

typedef enum   Ast_Unary_Kind   Ast_Unary_Kind;
typedef struct Ast_Unary        Ast_Unary;
struct Ast_Unary {
    Ast_Kind       node_kind;
    Ast_Unary_Kind unary_kind;
    Ast_Node      *arg;
};

typedef enum   Ast_Binary_Kind   Ast_Binary_Kind;
typedef struct Ast_Binary        Ast_Binary;
struct Ast_Binary {
    Ast_Kind        node_kind;
    Ast_Binary_Kind binary_kind;
    Ast_Node       *left;
    Ast_Node       *right;
};

union Ast_Node {
    Ast_Kind    node_kind;
    Ast_Literal literal;
    Ast_Unary   unary;
    Ast_Binary  binary;
};

LULU_INTERNAL_FUNC Ast_Node *
ast_literal_new(lulu_State *L,
    Arena           *arena,
    Ast_Literal_Kind kind,
    Value_Literal    value);

LULU_INTERNAL_FUNC Ast_Node *
ast_unary_new(lulu_State *L,
    Arena         *arena,
    Ast_Unary_Kind kind,
    Ast_Node      *arg);

LULU_INTERNAL_FUNC Ast_Node *
ast_binary_new(lulu_State *L,
    Arena          *arena,
    Ast_Binary_Kind kind,
    Ast_Node       *left,
    Ast_Node       *right);

LULU_INTERNAL_FUNC void
ast_print(const Ast_Node *n);

#endif // !LULU_AST_H
