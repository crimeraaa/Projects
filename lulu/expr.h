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
expr_make_literal(lulu_State *L, Type_Atom_Kind kind, const Token *token)
{
    const Type *type = type_atom_get(L, kind);
    Expr        expr = expr_make(Expr_Literal, type, token);
    return expr;
}

static inline Expr
expr_make_bool(lulu_State *L, const Token *token)
{
    Expr expr = expr_make_literal(L, Type_Atom_bool, token);
    expr.literal_bool = (token->kind == Token_true);
    return expr;
}

static inline Expr
expr_make_uint(lulu_State *L, const Token *token, lulu_uint literal)
{
    Expr expr = expr_make_literal(L, Type_Atom_uint, token);
    expr.literal_uint = literal;
    return expr;
}

static inline Expr
expr_make_real(lulu_State *L, const Token *token, lulu_real literal)
{
    Expr expr = expr_make_literal(L, Type_Atom_real, token);
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
        case Type_Atom_uint:
        case Type_Atom_int:
        case Type_Atom_real: return true;
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
expr_has_type_atom(Expr *e)
{
    return e->type != nullptr && e->type->kind == TypeKind_Atom;
}

#define expr_get_type_atom(e)   (LULU_ASSERT(expr_has_type_atom(e)), &e->type->atom)

static inline bool
expr_is_literal_type(Expr *e, Type_Atom_Kind kind)
{
    return expr_is_literal(e) && expr_get_type_atom(e)->kind == kind;
}
static inline bool
expr_is_literal_uint(Expr *e)
{
    return expr_is_literal_type(e, Type_Atom_uint);
}

static inline bool
expr_is_literal_bool(Expr *e)
{
    return expr_is_literal_type(e, Type_Atom_bool);
}

static inline bool
expr_is_literal_int(Expr *e)
{
    return expr_is_literal_type(e, Type_Atom_int);
}

static inline bool
expr_is_literal_real(Expr *e)
{
    return expr_is_literal_type(e, Type_Atom_real);
}

#define expr_get_uint(e) (LULU_ASSERT(expr_is_literal_uint(e)), e->literal_uint)
#define expr_get_int(e)  (LULU_ASSERT(expr_is_literal_int(e)),  e->literal_int)
#define expr_get_real(e) (LULU_ASSERT(expr_is_literal_real(e)), e->literal_real)
#define expr_get_reg(e)  (LULU_ASSERT(expr_is_reg(e)), e->reg)
#define expr_get_pc(e)   (LULU_ASSERT(expr_is_pc(e)), e->pc)

#endif // !LULU_EXPR_H
