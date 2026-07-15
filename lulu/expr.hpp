#pragma once

#include "internal.hpp"
#include "lexer.hpp"
#include "type.hpp"

enum ExprKind : u8 {
    Expr_None,
    Expr_nil,
    Expr_Literal,
    Expr_Type,       // Type name. No data.
    Expr_Constant,   // Constant index is in `constant`.
    Expr_Variable,   // Variable index is in `variable`.
    Expr_Call,       // Call instruction index in `pc`.
    Expr_Compare,    // Comparison instruction index in `pc`.
    Expr_Pending,    // Instruction index in `pc`- need destination register.
    Expr_Discharged, // Set to register `reg`.
};

struct Expr {
    ExprKind    kind;
    ValueKind   literal_kind; // Helps reduce pointer dereferencing.
    Token       token;
    Type const *type;
    union {
        Value literal;
        u32   constant; // Index of value in chunk constants array.
        i32   pc;       // Index of instruction in chunk bytecode array.
        u16   reg;      // Index of stack slot and/or active variable info.
    };
};

static inline Expr
expr_make(ExprKind kind, Type const *type, Token const &token)
{
    return {kind, Value_nil, token, type, {0}};
}


static inline Expr expr_make_none(void)
{
    return expr_make(Expr_None, nullptr, token_make_none());
}

static inline Expr
expr_make_nil(Token const &token)
{
    return expr_make(Expr_nil, basic_type_get(Value_nil), token);
}

// Literals are typed but can be coerced as long as data loss does not occur.
// Coercion only occurs for numeric types.
template<class T>
static inline Expr
expr_make_literal(Token const &token, T arg)
{
    ValueKind kind = trait_ValueKind<T>::kind;
    Expr      expr = expr_make(Expr_Literal, nullptr, token);

    expr.type         = basic_type_get(kind);
    expr.literal_kind = kind;
    value_set(&expr.literal, arg);
    return expr;
}

static inline Expr expr_make_bool(Token const &t) { return expr_make_literal(t, t.kind == Token_true); }
static inline Expr expr_make_int (Token const &t, lulu_int  i) { return expr_make_literal(t, i); }
static inline Expr expr_make_real(Token const &t, lulu_real r) { return expr_make_literal(t, r); }

static inline Expr
expr_make_type(Token const &token, Type const *type)
{
    return expr_make(Expr_Type, type, token);
}

static inline Expr
expr_make_variable(Token const &token, Type const *type, u16 reg)
{
    Expr e = expr_make(Expr_Variable, type, token);
    e.reg = reg;
    return e;
}

static inline bool
expr_is_numeric(Expr *e)
{
    switch (e->type->kind) {
    case TypeKind_Basic:
        switch (e->type->basic.kind) {
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

#define expr_is_literal(e) ((e)->kind == Expr_Literal)

static inline bool
expr2_both_literal(Expr *a, Expr *b)
{
    return expr_is_literal(a) && expr_is_literal(b);
}

static inline bool expr_is_reg    (Expr *a) { return a->kind == Expr_Discharged; }
static inline bool expr_is_local  (Expr *e) { return e->kind == Expr_Variable;   }
static inline bool expr_is_compare(Expr *e) { return e->kind == Expr_Compare;    }
static inline bool expr_is_pc     (Expr *a) { return a->kind == Expr_Pending;    }

static inline bool expr_has_basic_type(Expr *e)
{
    return e->type != nullptr && e->type->kind == TypeKind_Basic;
}

static inline bool
expr_has_basic_kind(Expr *e, ValueKind kind)
{
    return expr_has_basic_type(e) && e->type->basic.kind == kind;
}

#define expr_basic_kind(e) \
    (LULU_ASSERT(expr_has_basic_type(e)), (e)->type->basic.kind)

static inline bool
expr_has_literal_kind(Expr *e, ValueKind kind)
{
    return expr_is_literal(e) && (e->literal_kind == kind);
}

static inline bool expr_is_bool(Expr *e) { return expr_has_literal_kind(e, Value_bool); }
static inline bool expr_is_int (Expr *e) { return expr_has_literal_kind(e, Value_int);  }
static inline bool expr_is_real(Expr *e) { return expr_has_literal_kind(e, Value_real); }

#define expr_literal_kind(e) (LULU_ASSERT(expr_is_literal(e)),  (e)->literal_kind)
#define expr_bool(e)         (LULU_ASSERT(expr_is_bool(e)),     value_bool((e)->literal))
#define expr_int(e)          (LULU_ASSERT(expr_is_int(e)),      value_int ((e)->literal))
#define expr_real(e)         (LULU_ASSERT(expr_is_real(e)),     value_real((e)->literal))
#define expr_reg(e)          (LULU_ASSERT(expr_is_reg(e)),      (e)->reg)
#define expr_local(e)        (LULU_ASSERT(expr_is_local(e)),    (e)->reg)
#define expr_compare(e)      (LULU_ASSERT(expr_is_compare(e)),  (e)->pc)
#define expr_pc(e)           (LULU_ASSERT(expr_is_pc(e)),       (e)->pc)

template<class T>
static inline void
expr_set(Expr *e, T arg)
{
    e->literal_kind = trait_ValueKind<T>::kind;
    e->type         = basic_type_get(e->literal_kind);
    value_set<T>(&e->literal, arg);
}

static inline void expr_set_bool(Expr *e, bool      b) { expr_set(e, b); }
static inline void expr_set_int (Expr *e, lulu_int  i) { expr_set(e, i); }
static inline void expr_set_real(Expr *e, lulu_real r) { expr_set(e, r); }

// Necessary for template shenanigans.
template<class T>
inline T
expr_literal(Expr *e);

template<> inline bool      expr_literal(Expr *e) { return expr_bool(e); }
template<> inline lulu_int  expr_literal(Expr *e) { return expr_int (e); }
template<> inline lulu_real expr_literal(Expr *e) { return expr_real(e); }

