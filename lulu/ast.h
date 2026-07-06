#ifndef LULU_AST_H
#define LULU_AST_H

#include "lulu.h"
#include "internal.h"
#include "lexer.h"
#include "value.h"

// Safety net to avoid stack overflows due to recursion.
#define AST_MAX_RECURSIONS  250

/*
 Description:
    Macro hell!

 NIT(2026-07-04):
    We are using a horrible combination of PascalCase and snake_case:
    PascalCase_SnakeCase!

    Because visually parsing the tagged union's tag and actual variant
    type can be confusing at times.
 */
#define AST_KINDS(X)                                                           \
    X(None, "none", struct {int _;})                                           \
    X(Literal, "literal", struct {                                             \
        Token         token;                                                   \
        Value_Literal value;                                                   \
    })                                                                         \
    X(Ident, "ident", struct {                                                 \
        u32   hash;                                                            \
        Token token;                                                           \
    })                                                                         \
    X(ParenExpr, "paren", struct {                                             \
        Token open;                                                            \
        Token close;                                                           \
        Ast * expr;                                                            \
    })                                                                         \
    /* Could also be a cast, e.g. `u64(x)` or `(uintptr)(p)`. */               \
    X(CallExpr, "call", struct {                                               \
        Token     open;  /* The opening '('. */                                \
        Token     close; /* The closing ')'. */                                \
        Ast *     func;                                                        \
        Slice_Ast args;                                                        \
    })                                                                         \
    X(CastExpr, "cast", struct {                                               \
        Token op; /* 'cast' itself. */                                         \
        Ast * type;                                                            \
        Ast * expr;                                                            \
    })                                                                         \
    X(UnaryExpr, "unary", struct {                                             \
        Token op;                                                              \
        Ast * arg;                                                             \
    })                                                                         \
    X(BinaryExpr, "binary", struct {                                           \
        Token op;                                                              \
        Ast * left;                                                            \
        Ast * right;                                                           \
    })                                                                         \
    X(AssignStmt, "assign", struct {                                           \
        Token     op;    /* The '=' operator. */                               \
        Slice_Ast left;  /* Expression/s before '='. */                        \
        Slice_Ast right; /* Expression/s after '='. */                         \
    })                                                                         \
    X(DeclStmt, "decl", struct {                                               \
        Slice_Ast names;  /* Expression before '='. */                         \
        Ast *     type;   /* Expression after ':' but before '='. */           \
        Slice_Ast values; /* Expression after '=' to assign `name` with. */    \
    })                                                                         \


enum AstKind {
#define X(e, ...) AstKind_##e,
    AST_KINDS(X)
#undef X
};


typedef enum   AstKind AstKind;
typedef struct Ast     Ast;

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


struct Ast {
    // Use this to select the actual, concrete member below.
    AstKind kind;
    union {
#define X(e, ...) Ast_##e e;
        AST_KINDS(X)
#undef X
    };
};

LULU_INTERNAL_FUNC const char *
ast_kind_cstring(AstKind k);

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

