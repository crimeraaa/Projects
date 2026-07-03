#ifndef LULU_AST_H
#define LULU_AST_H

#include "lulu.h"
#include "internal.h"
#include "arena.h"
#include "lexer.h"
#include "value.h"

// Safety net to avoid stack overflows due to recursion.
#define AST_MAX_RECURSIONS  250

// This is absolutely atrocious!
#define AST_KINDS(X)                                                           \
    X(None, "none", struct {int _;})                                           \
    X(Literal, "literal", struct {                                             \
        Ast_Kind      kind;                                                    \
        Token         token;                                                   \
        Value_Literal value;                                                   \
    })                                                                         \
    X(Ident, "ident", struct {                                                 \
        Ast_Kind kind;                                                         \
        u32      hash;                                                         \
        Token    token;                                                        \
    })                                                                         \
    X(Paren_Expr, "paren", struct {                                            \
        Ast_Kind kind;                                                         \
        Token    open;                                                         \
        Token    close;                                                        \
        Ast *    expr;                                                         \
    })                                                                         \
    /* Could also be a cast, e.g. `u64(x)` or `(uintptr)(p)`. */               \
    X(Call_Expr, "call", struct {                                              \
        Ast_Kind  kind;                                                        \
        Token     open;  /* The opening '('. */                                \
        Token     close; /* The closing ')'. */                                \
        Ast *     func;                                                        \
        Slice_Ast args;                                                        \
    })                                                                         \
    X(Cast_Expr, "cast", struct {                                              \
        Ast_Kind  kind;                                                        \
        Token     op; /* 'cast' itself. */                                     \
        Ast *     type;                                                        \
        Ast *     expr;                                                        \
    })                                                                         \
    X(Unary_Expr, "unary", struct {                                            \
        Ast_Kind  kind;                                                        \
        Token     op;                                                          \
        Ast *     arg;                                                         \
    })                                                                         \
    X(Binary_Expr, "binary", struct {                                          \
        Ast_Kind  kind;                                                        \
        Token     op;                                                          \
        Ast *     left;                                                        \
        Ast *     right;                                                       \
    })                                                                         \
    X(Assign_Stmt, "assign", struct {                                          \
        Ast_Kind  kind;                                                        \
        Token     op;    /* The '=' operator. */                               \
        Slice_Ast left;  /* Expression/s before '='. */                        \
        Slice_Ast right; /* Expression/s after '='. */                         \
    })                                                                         \
    X(Decl_Stmt, "decl", struct {                                              \
        Ast_Kind  kind;                                                        \
        Slice_Ast names;  /* Expression before '='. */                         \
        Ast *     type;   /* Expression after ':' but before '='. */           \
        Slice_Ast values; /* Expression after '=' to assign `name` with. */    \
    })                                                                         \


enum Ast_Kind {
#define X(e, ...) Ast_Kind_##e,
    AST_KINDS(X)
#undef X
};


typedef enum   Ast_Kind  Ast_Kind;
typedef union  Ast       Ast;

/*
 NIT(2026-07-03):
    One of the few cases I would love to have C++ style templates.
 */
typedef struct Slice_Ast Slice_Ast;
struct Slice_Ast {
    Ast **data; // 1D array of `Ast *`.
    usize len;
};

#define X(e, s, t) typedef t Ast_##e;
// AST concrete type definitions.
AST_KINDS(X)
#undef X


union Ast {
#define X(e, ...) Ast_##e e;
    // Use this to select the actual, concrete member below.
    Ast_Kind kind;
    AST_KINDS(X)
#undef X
};

LULU_INTERNAL_FUNC const char *
ast_kind_cstring(Ast_Kind k);

LULU_INTERNAL_FUNC Ast *
ast_literal_new(lulu_State *L, const Token *token);

LULU_INTERNAL_FUNC Ast *
ast_ident_new(lulu_State *L, const Token *token);

LULU_INTERNAL_FUNC Ast *
ast_paren_new(lulu_State *L,
    const Token *open,
    const Token *close,
    Ast *        expr);

LULU_INTERNAL_FUNC Ast *
ast_cast_new(lulu_State *L, const Token *op, Ast *type, Ast *expr);

LULU_INTERNAL_FUNC Ast *
ast_call_new(lulu_State *L,
    const Token *open,
    const Token *close,
    Ast *        func,
    Slice_Ast    args);

LULU_INTERNAL_FUNC Ast *
ast_unary_new(lulu_State *L, const Token *op, Ast *arg);

LULU_INTERNAL_FUNC Ast *
ast_binary_new(lulu_State *L, const Token *op, Ast *left, Ast *right);

LULU_INTERNAL_FUNC Ast *
ast_decl_new(lulu_State *L, Slice_Ast names, Ast *type, Slice_Ast values);

LULU_INTERNAL_FUNC Ast *
ast_assign_new(lulu_State *L,
    const Token *op,
    Slice_Ast    left,
    Slice_Ast    right);

LULU_INTERNAL_FUNC void
ast_print(Ast *a);

#endif // !LULU_AST_H

