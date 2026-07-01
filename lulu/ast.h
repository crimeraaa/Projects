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
    AST_PAREN,
    AST_DECL,
    AST_ASSIGN,
    AST_CALL, // Could also be a cast, e.g. `u64(x)`.
    AST_CAST,
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
    Ast_Kind kind;
    Token    open;
    Token    close;
    Ast *    expr;
};

typedef struct Ast_Decl Ast_Decl;
struct Ast_Decl {
    Ast_Kind kind;
    Ast *    name;  // Expression before '='.
    Ast *    type;  // Expression after ':' but before '='.
    Ast *    value; // Expression after '=' to assign `name` with.
};

typedef struct Ast_Assign Ast_Assign;
struct Ast_Assign {
    Ast_Kind kind;
    Token    op;    // The '=' operator.
    Ast *    left;  // Expression/s before '='.
    Ast *    right; // Expression/s after '='.
};

typedef struct Ast_Call Ast_Call;
struct Ast_Call {
    Ast_Kind kind;
    Token    open;  // The opening '('.
    Token    close; // The closing ')'.
    Ast *    func;
    Ast *    arg;   // TODO(2026-07-02): Use an array instead
};

typedef struct Ast_Cast Ast_Cast;
struct Ast_Cast {
    Ast_Kind  kind;
    Token     op; // 'cast' itself.
    Ast *     type;
    Ast *     expr;
};

typedef struct Ast_Unary Ast_Unary;
struct Ast_Unary {
    Ast_Kind  kind;
    Token     op;
    Ast *     arg;
};

typedef struct Ast_Binary Ast_Binary;
struct Ast_Binary {
    Ast_Kind  kind;
    Token     op;
    Ast *     left;
    Ast *     right;
};

union Ast {
    // Use this to select the actual, concrete member below.
    Ast_Kind    kind;
    Ast_Literal literal;
    Ast_Ident   ident;
    Ast_Decl    decl;
    Ast_Assign  assign_stmt;
    Ast_Paren   paren_expr;
    Ast_Call    call_expr;
    Ast_Cast    cast_expr;
    Ast_Unary   unary_expr;
    Ast_Binary  binary_expr;
};

LULU_INTERNAL_FUNC Ast *
ast_literal_new(lulu_State *L, const Token *token);

LULU_INTERNAL_FUNC Ast *
ast_ident_new(lulu_State *L, const Token *token);

LULU_INTERNAL_FUNC Ast *
ast_paren_new(lulu_State *L, const Token *open, const Token *close, Ast *expr);

LULU_INTERNAL_FUNC Ast *
ast_decl_new(lulu_State *L, Ast *name, Ast *type, Ast *value);

LULU_INTERNAL_FUNC Ast *
ast_assign_new(lulu_State *L, const Token *op, Ast *left, Ast *right);

LULU_INTERNAL_FUNC Ast *
ast_cast_new(lulu_State *L, const Token *op, Ast *type, Ast *expr);

LULU_INTERNAL_FUNC Ast *
ast_call_new(lulu_State *L, const Token *open, const Token *close, Ast *func, Ast *arg);

LULU_INTERNAL_FUNC Ast *
ast_unary_new(lulu_State *L, const Token *op, Ast *arg);

LULU_INTERNAL_FUNC Ast *
ast_binary_new(lulu_State *L, const Token *op, Ast *left, Ast *right);

LULU_INTERNAL_FUNC void
ast_print(const Ast *a);

#if 0
typedef struct Slice_Ast Slice_Ast;
struct Slice_Ast {
    Ast **data; // 1D array of `Ast *`.
    usize len;
};

LULU_INTERNAL_FUNC void
slice_ast_append(lulu_State *L, Slice_Ast *s, Ast *a);
#endif

#endif // !LULU_AST_H
