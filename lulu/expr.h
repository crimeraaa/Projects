#ifndef LULU_EXPR_H
#define LULU_EXPR_H

#include "lulu.h"
#include "internal.h"
#include "lexer.h"
#include "type.h"

typedef enum ExprKind {
    Expr_None,
    Expr_nil,
    Expr_Literal,
    Expr_Type,       // Type name. No data.
    Expr_Variable,   // Variable index is in `variable`.
    Expr_Constant,   // Constant index is in `constant`.
    Expr_Pending,    // Instruction index in `pc`- need destination register.
    Expr_Call,       // Call instruction index in `pc`.
    Expr_Discharged, // Set to register `reg`.
} ExprKind;

typedef struct Expr Expr;
struct Expr {
    ExprKind    kind;
    ValueKind   literal_kind;
    Type const *type;
    Token       token;
    union {
        lulu_uint literal_uint;
        lulu_int  literal_int;
        lulu_real literal_real;
        lulu_bool literal_bool;

        u32 constant; // Index of value in chunk constants array.
        i32 pc;       // Index of instruction in chunk bytecode array.
        u16 reg;      // Index of stack slot and/or active variable info.
    };
};

static inline Expr
expr_make(ExprKind kind,
    ValueKind      literal_kind,
    Type  const *  type,
    Token const *  token)
{
    Expr expr = {kind, literal_kind, type, *token, {0}};
    return expr;
}


static inline Expr
expr_make_none(void)
{
    Token token = token_make_none();
    return expr_make(Expr_None, Value_nil, nullptr, &token);
}

static inline Expr
expr_make_nil(Token const *token)
{
    return expr_make(Expr_nil, Value_nil, nullptr, token);
}

static inline Expr
expr_make_literal(ValueKind kind, Token const *token)
{
    Type const *type = atom_type_get(kind);
    return expr_make(Expr_Literal, kind, type, token);
}

static inline Expr
expr_make_bool(Token const *token)
{
    Expr expr = expr_make_literal(Value_bool, token);
    expr.literal_bool = (token->kind == Token_true);
    return expr;
}

static inline Expr
expr_make_uint(Token const *token, lulu_uint literal)
{
    Expr expr = expr_make_literal(Value_uint, token);
    expr.literal_uint = literal;
    return expr;
}

static inline Expr
expr_make_real(Token const *token, lulu_real literal)
{
    Expr expr = expr_make_literal(Value_real, token);
    expr.literal_real = literal;
    return expr;
}

static inline Expr
expr_make_type(Token const *token, Type const *type)
{
    return expr_make(Expr_Type, Value_nil, type, token);
}

static inline Expr
expr_make_variable(Token const *token, Type const *type, u16 reg)
{
    Expr e = expr_make(Expr_Variable, Value_nil, type, token);
    e.reg = reg;
    return e;
}

static inline bool
expr_is_numeric(Expr *e)
{
    switch (e->type->kind) {
    case TypeKind_Atom:
        switch (e->type->atom.kind) {
        case Value_uint:
        case Value_int:
        case Value_real: return true;
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
expr_has_atom_kind(Expr *e, ValueKind kind)
{
    return expr_has_atom_type(e) && e->type->atom.kind == kind;
}

#define expr_atom_kind(e) (LULU_ASSERT(expr_has_atom_type(e)), (e)->type->atom.kind)

static inline bool
expr_is_literal_type(Expr *e, ValueKind kind)
{
    return expr_is_literal(e) && e->type->atom.kind == kind;
}

static inline bool
expr_is_literal_uint(Expr *e)
{
    return expr_is_literal_type(e, Value_uint);
}

static inline bool
expr_is_literal_bool(Expr *e)
{
    return expr_is_literal_type(e, Value_bool);
}

static inline bool
expr_is_literal_int(Expr *e)
{
    return expr_is_literal_type(e, Value_int);
}

static inline bool
expr_is_literal_real(Expr *e)
{
    return expr_is_literal_type(e, Value_real);
}

#define expr_literal_kind(e)    (LULU_ASSERT(expr_is_literal(e), e->literal_kind)
#define expr_uint(e)            (LULU_ASSERT(expr_is_literal_uint(e)), e->literal_uint)
#define expr_int(e)             (LULU_ASSERT(expr_is_literal_int(e)),  e->literal_int)
#define expr_real(e)            (LULU_ASSERT(expr_is_literal_real(e)), e->literal_real)
#define expr_reg(e)             (LULU_ASSERT(expr_is_reg(e)), e->reg)
#define expr_pc(e)              (LULU_ASSERT(expr_is_pc(e)), e->pc)

#endif // !LULU_EXPR_H
