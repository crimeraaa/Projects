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
    Expr_Local,      // Variable index is in `reg`.
    Expr_Call,       // Call instruction index in `pc`.
    Expr_Compare,    // Comparison instruction index in `pc`.
    Expr_Pending,    // Instruction index in `pc`- need destination register.
    Expr_Discharged, // Set to register `reg`.
};

struct Expr {
    ExprKind    kind         = Expr_None;
    ValueKind   literal_kind = Value_nil; // Helps reduce pointer dereferencing.
    bool        literal_sign = false;     // Integer literal shenanigans.
    Token       token;
    Type const *type         = nullptr;
    union {
        Value literal = {0};
        u32   constant; // Index of value in chunk constants array.
        i32   pc;       // Index of instruction in chunk bytecode array.
        u16   reg;      // Index of stack slot and/or active variable info.
    };
};

static inline Expr
expr_make(ExprKind kind, Type const *type, Token const &token)
{
    Expr e;
    e.kind  = kind;
    e.type  = type;
    e.token = token;
    return e;
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
    auto constexpr    kind = trait_ValueKind<T>;
    Type const *const type = basic_type_get(kind);
    Expr              expr = expr_make(Expr_Literal, type, token);

    // Literal value stuff
    expr.literal_kind = kind;
    if constexpr(kind == Value_int) {
        expr.literal_sign = arg < 0;
    }
    value_set(&expr.literal, arg);
    return expr;
}

static inline Expr expr_make_bool(Token const &t, bool      b) { return expr_make_literal(t, b); }
static inline Expr expr_make_int (Token const &t, lulu_int  i) { return expr_make_literal(t, i); }
static inline Expr expr_make_real(Token const &t, lulu_real r) { return expr_make_literal(t, r); }

static inline Expr
expr_make_type(Token const &token, Type const *type)
{
    return expr_make(Expr_Type, type, token);
}

static inline Expr
expr_make_local(Token const &token, Type const *type, u16 reg)
{
    Expr e = expr_make(Expr_Local, type, token);
    e.reg = reg;
    return e;
}

static inline bool
expr_is_numeric(Expr const *e)
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
expr2_both_literal(Expr const *a, Expr const *b)
{
    return expr_is_literal(a) && expr_is_literal(b);
}

static inline bool expr_is_reg    (Expr const *e) { return e->kind == Expr_Discharged; }
static inline bool expr_is_local  (Expr const *e) { return e->kind == Expr_Local;   }
static inline bool expr_is_compare(Expr const *e) { return e->kind == Expr_Compare;    }
static inline bool expr_is_pc     (Expr const *e) { return e->kind == Expr_Pending;    }

static inline bool expr_has_basic_type(Expr const *e)
{
    return e->type != nullptr && e->type->kind == TypeKind_Basic;
}

static inline bool
expr_has_basic_kind(Expr const *e, ValueKind kind)
{
    return expr_has_basic_type(e) && e->type->basic.kind == kind;
}

#define expr_basic_kind(e) \
    (LULU_ASSERT(expr_has_basic_type(e)), (e)->type->basic.kind)

static inline bool
expr_has_literal_kind(Expr const *e, ValueKind kind)
{
    return expr_is_literal(e) && (e->literal_kind == kind);
}

static inline bool expr_is_bool(Expr const *e) { return expr_has_literal_kind(e, Value_bool); }
static inline bool expr_is_int (Expr const *e) { return expr_has_literal_kind(e, Value_int);  }
static inline bool expr_is_real(Expr const *e) { return expr_has_literal_kind(e, Value_real); }

#define expr_literal_kind(e) (LULU_ASSERT(expr_is_literal(e)),  (e)->literal_kind)
#define expr_bool(e)         (LULU_ASSERT(expr_is_bool(e)),     value_bool((e)->literal))
#define expr_int(e)          (LULU_ASSERT(expr_is_int(e)),      value_int ((e)->literal))
#define expr_real(e)         (LULU_ASSERT(expr_is_real(e)),     value_real((e)->literal))
#define expr_reg(e)          (LULU_ASSERT(expr_is_reg(e)),      (e)->reg)
#define expr_pc(e)           (LULU_ASSERT(expr_is_pc(e)),       (e)->pc)

template<class T>
static inline void
expr_set(Expr *e, T arg)
{
    e->literal_kind = trait_ValueKind<T>;
    e->type         = basic_type_get(e->literal_kind);
    value_set<T>(&e->literal, arg);
}

static inline void expr_set_bool(Expr *e, bool      b) { expr_set(e, b); }
static inline void expr_set_int (Expr *e, lulu_int  i) { expr_set(e, i); }
static inline void expr_set_real(Expr *e, lulu_real r) { expr_set(e, r); }

// Necessary for template shenanigans.
template<class T>
inline T
expr_literal(Expr const *e);

template<> inline bool      expr_literal(Expr const *e) { return expr_bool(e); }
template<> inline lulu_int  expr_literal(Expr const *e) { return expr_int (e); }
template<> inline lulu_real expr_literal(Expr const *e) { return expr_real(e); }

static bool
expr_neg(Expr *e)
{
    switch (expr_literal_kind(e)) {
    case Value_int:  value_set_int (&e->literal, -expr_int (e)); break;
    case Value_real: value_set_real(&e->literal, -expr_real(e)); break;
    default:
        return false;
    }
    return true;
}

// To coerce (i.e. implicily cast) the expression from the source type to the
// destination type. This modifies the expression in-ploce if successful.
template<class Src, class Dst>
static bool
expr_coerce(Expr *e)
{
    LULU_ASSERT(expr_is_literal(e));
    auto src_arg = value_get<Src>(e->literal);
    auto dst_arg = cast(Dst)src_arg;

    // Conversion results in data loss?
    if (cast(Src)dst_arg != src_arg) {
        return false;
    }

    expr_set<Dst>(e, dst_arg);
    return true;
}

static bool
expr_int_safe(Expr *e, lulu_int min, lulu_int max, lulu_int *out)
{
    lulu_int imm = 0;
    switch (expr_literal_kind(e)) {
    case Value_int:
        imm = expr_int(e);
        break;
    case Value_real: {
        lulu_real r = expr_real(e);

        // Conversion results in data loss?
        imm = cast(lulu_int)r;
        if (cast(lulu_real)imm != r) {
            return false;
        }
        break;
    }
    default:
        return false;
    }

    *out = imm;
    return min <= imm && imm <= max;
}
