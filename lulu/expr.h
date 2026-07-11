#ifndef LULU_EXPR_H
#define LULU_EXPR_H

#include "lulu.h"
#include "internal.h"
#include "lexer.h"
#include "type.h"

typedef enum ExprKind {
    Expr_None,

    // Literal expressions
    Expr_nil,
    Expr_Literal,

    // Constant index is in `constant`.
    Expr_Constant,

    // Expression has been set to a register `reg`.
    Expr_Discharged,

    // Expression has an instruction in `pc` but needs the destination
    // register to be finalized.
    Expr_Pending,
} ExprKind;

typedef struct Expr Expr;
struct Expr {
    ExprKind    kind;
    const Type *type;
    Token       token;
    union {
        lulu_uint literal_uint;
        lulu_int  literal_int;
        lulu_real literal_real;
        bool      literal_bool;

        // Absolute index of a value in the chunk's constants array.
        u32 constant;

        // Absolute index of an instruction in the chunk's bytecode array.
        i32 pc;

        // Absolute stack slot.
        u8 reg;
    };
};

static inline Expr
expr_make(ExprKind kind, const Type *type, const Token *token)
{
    Expr expr = {kind, type, *token, {0}};
    return expr;
}


static inline Expr
expr_make_none(void)
{
    const Token token = token_make_none();
    return expr_make(Expr_None, nullptr, &token);
}

static inline Expr
expr_make_nil(const Token *token)
{
    return expr_make(Expr_nil, nullptr, token);
}

static inline Expr
expr_make_literal(AtomKind kind, const Token *token)
{
    const Type *type = atom_type_get(kind);
    return expr_make(Expr_Literal, type, token);
}

static inline Expr
expr_make_bool(const Token *token)
{
    Expr expr = expr_make_literal(Atom_bool, token);
    expr.literal_bool = (token->kind == Token_true);
    return expr;
}

static inline Expr
expr_make_uint(const Token *token, lulu_uint literal)
{
    Expr expr = expr_make_literal(Atom_uint, token);
    expr.literal_uint = literal;
    return expr;
}

static inline Expr
expr_make_real(const Token *token, lulu_real literal)
{
    Expr expr = expr_make_literal(Atom_real, token);
    expr.literal_real = literal;
    return expr;
}

static inline bool
expr_is_numeric(Expr *e)
{
    LULU_ASSERT(e->type != nullptr);
    switch (e->type->kind) {
    case TypeKind_Atom:
        switch (e->type->atom.kind) {
        case Atom_uint:
        case Atom_int:
        case Atom_real: return true;
        default:
            break;
        }
        break;
    default:
        LULU_UNIMPLEMENTED();
        break;
    }
    return false;
}

static inline void
expr_set_constant(Expr *e, u32 index)
{
    e->kind     = Expr_Constant;
    e->constant = index;
}

static inline bool
expr_is_literal(Expr *a)
{
    return a->kind == Expr_Literal;
}

static inline bool
expr2_both_literal(Expr *a, Expr *b)
{
    return expr_is_literal(a) && expr_is_literal(b);
}

static inline bool
expr_is_reg(Expr *a)
{
    return a->kind == Expr_Discharged;
}

static inline bool
expr_is_pc(Expr *a)
{
    return a->kind == Expr_Pending;
}

static inline bool
expr_has_atom_type(Expr *e)
{
    return e->type != nullptr && e->type->kind == TypeKind_Atom;
}

static inline bool
expr_has_atom_kind(Expr *e, AtomKind kind)
{
    return expr_has_atom_type(e) && e->type->atom.kind == kind;
}

#define expr_get_atom_kind(e) \
    (LULU_ASSERT(expr_has_atom_type(e)),\
     (e)->type->atom.kind)

static inline bool
expr_is_literal_type(Expr *e, AtomKind kind)
{
    return expr_is_literal(e) && e->type->atom.kind == kind;
}

static inline bool
expr_is_literal_uint(Expr *e)
{
    return expr_is_literal_type(e, Atom_uint);
}

static inline bool
expr_is_literal_bool(Expr *e)
{
    return expr_is_literal_type(e, Atom_bool);
}

static inline bool
expr_is_literal_int(Expr *e)
{
    return expr_is_literal_type(e, Atom_int);
}

static inline bool
expr_is_literal_real(Expr *e)
{
    return expr_is_literal_type(e, Atom_real);
}

#define expr_get_uint(e) (LULU_ASSERT(expr_is_literal_uint(e)), e->literal_uint)
#define expr_get_int(e)  (LULU_ASSERT(expr_is_literal_int(e)),  e->literal_int)
#define expr_get_real(e) (LULU_ASSERT(expr_is_literal_real(e)), e->literal_real)
#define expr_get_reg(e)  (LULU_ASSERT(expr_is_reg(e)), e->reg)
#define expr_get_pc(e)   (LULU_ASSERT(expr_is_pc(e)), e->pc)

#endif // !LULU_EXPR_H
