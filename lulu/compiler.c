#include <math.h> // trunc

#include "compiler.h"
#include "chunk.h"
#include "expr.h"
#include "internal.h"
#include "opcode.h"
#include "type.h"
#include "value.h"

LULU_NORETURN static void
compiler_error(Compiler *c, char const *info, Expr const *e)
{
    parser_error_at(c->parser, info, &e->token);
}

static i32
compiler_code(Compiler *c, Instruction i)
{
    i32 pc = cast(i32)c->chunk->code_len;
    chunk_add_instruction(c->L, c->chunk, i);
    return pc;
}

static i32
compiler_code_ABC(Compiler *c, OpCode Op, u16 A, u16 B, u16 C)
{
    LULU_ASSERT(opcode_is_ABC(Op));
    LULU_ASSERT(A <= ARG_A_MAX);
    LULU_ASSERT(B <= ARG_B_MAX);
    LULU_ASSERT(C <= ARG_C_MAX);
    return compiler_code(c, MAKE_ABC(Op, A, B, C));
}

static i32
compiler_code_vABC(Compiler *c, OpCode Op, u16 A, u16 B, u16 vC, bool k)
{
    LULU_ASSERT(opcode_is_vABC(Op));
    LULU_ASSERT(A <= ARG_A_MAX);
    LULU_ASSERT(B  <= ARG_B_MAX);
    LULU_ASSERT(vC <= ARG_vC_MAX);
    return compiler_code(c, MAKE_vABC(Op, A, B, vC, k));
}

static i32
compiler_code_ABx(Compiler *c, OpCode Op, u16 A, u32 Bx)
{
    LULU_ASSERT(opcode_is_ABx(Op));
    LULU_ASSERT(A <= ARG_A_MAX);
    LULU_ASSERT(Bx <= ARG_Bx_MAX);
    return compiler_code(c, MAKE_ABx(Op, A, Bx));
}

static i32
compiler_code_AsBx(Compiler *c, OpCode Op, u16 A, i32 sBx)
{
    LULU_ASSERT(opcode_is_AsBx(Op));
    LULU_ASSERT(A <= ARG_A_MAX);
    LULU_ASSERT(ARG_sBx_MIN <= sBx && sBx <= ARG_sBx_MAX);
    return compiler_code(c, MAKE_AsBx(Op, A, sBx));
}

#define expr_set_literal(e, T, v)                                              \
do {                                                                           \
    (e)->kind         = Expr_Literal;                                          \
    (e)->type         = basic_type_get(Value_##T);                             \
    (e)->literal_kind = Value_##T;                                             \
    (e)->literal_##T  = v;                                                     \
} while (0)

static void expr_set_bool_literal(Expr *e, lulu_bool b) { expr_set_literal(e, bool, b); }
static void expr_set_uint_literal(Expr *e, lulu_uint u) { expr_set_literal(e, uint, u); }
static void expr_set_int_literal (Expr *e, lulu_int  i) { expr_set_literal(e, int,  i); }
static void expr_set_real_literal(Expr *e, lulu_real r) { expr_set_literal(e, real, r); }

#undef expr_set_literal

// I'm not even going to pretend this is remotely sane
// Marginal waste of time when the type matches its literals, but that's
// not common behavior anyway.
#define compiler_cast_type(c, e, T)                                            \
    ValueKind vk = expr_basic_kind(e);                                         \
    switch (e->kind) {                                                         \
    case Expr_Literal:                                                         \
        switch (vk) {                                                          \
        case Value_bool:                                                       \
            expr_set_##T##_literal(e, cast(lulu_##T)e->literal_bool);          \
            return;                                                            \
        case Value_uint:                                                       \
            expr_set_##T##_literal(e, cast(lulu_##T)e->literal_uint);          \
            return;                                                            \
        case Value_int:                                                        \
            expr_set_##T##_literal(e, cast(lulu_##T)e->literal_int);           \
            return;                                                            \
        case Value_real:                                                       \
            expr_set_##T##_literal(e, cast(lulu_##T)e->literal_real);          \
            return;                                                            \
        default:                                                               \
            break;                                                             \
        }                                                                      \
        break;                                                                 \
    case Expr_Pending: compiler_expr_any_reg(c, e); /* fallthrough */          \
    case Expr_Discharged: {                                                    \
        ValueKind tag = Value_##T;                                             \
        compiler_code_ABC(c, Op_cast, e->reg, cast(u16)tag, cast(u16)vk);      \
        e->type = basic_type_get(tag);                                         \
        return;                                                                \
    }                                                                          \
    default:                                                                   \
        break;                                                                 \
    }                                                                          \
    compiler_error(c, "Cannot cast to '" #T "'", e);


// Wrapper functions.
static void compiler_cast_bool(Compiler *c, Expr *e) { compiler_cast_type(c, e, bool); }
static void compiler_cast_uint(Compiler *c, Expr *e) { compiler_cast_type(c, e, uint); }
static void compiler_cast_int (Compiler *c, Expr *e) { compiler_cast_type(c, e, int);  }
static void compiler_cast_real(Compiler *c, Expr *e) { compiler_cast_type(c, e, real); }

#undef compiler_cast_type

LULU_INTERNAL_FUNC void
compiler_cast(Compiler *c, Type const *t, Expr *e)
{   
    // Nothing to do?
    if (e->type == t) {
        return;
    }

    switch (t->kind) {
    case TypeKind_Basic:
        switch (t->basic.kind) {
        case Value_bool: compiler_cast_bool(c, e); return;
        case Value_int:  compiler_cast_int (c, e); return;
        case Value_uint: compiler_cast_uint(c, e); return;
        case Value_real: compiler_cast_real(c, e); return;
        default:
            break;
        }
        break;
    default:
        // Need dedicated instructions!
        break;
    }
    compiler_error(c, "Cannot cast to type", e);
}

LULU_INTERNAL_FUNC void
compiler_call(Compiler *c, Expr *func, Expr *arg)
{
    if (func->kind != Expr_Type) {
        compiler_error(c, "Function calls not yet supported", func);
    }
    compiler_cast(c, func->type, arg);
}

static void
reg_pop(Compiler *c, u16 reg)
{
    if (reg >= c->active_count) {
        c->free_reg--;
        LULU_ASSERTF(reg == c->free_reg, "Expected free_reg = %u but got %u", reg, c->free_reg);
    }
}

static void
reg_push(Compiler *c, u16 reg_count)
{
    // TODO(2026-07-07): Check stack size!
    c->free_reg += reg_count;
    if (c->free_reg > c->chunk->stack_size) {
        c->chunk->stack_size = c->free_reg;
    }
}

static void
expr_pop(Compiler *c, Expr *e)
{
    if (e->kind == Expr_Discharged) {
        reg_pop(c, e->reg);
    }
}

static void
expr_discharge_vars(Compiler *c, Expr *e)
{
    // TODO(2026-07-13): Differentiate globals and locals?
    switch (e->kind) {
    case Expr_Variable:
        e->kind = Expr_Discharged;
        break;
    default:
        break;
    }
}

static void
compiler_load_bool(Compiler *c, u16 reg, bool b, bool skip)
{
    compiler_code_vABC(c, Op_bool, reg, cast(u16)b, 0, skip);
}

static void
compiler_load_uint(Compiler *c, u16 reg, lulu_uint u)
{
    if (u <= ARG_Bx_MAX) {
        compiler_code_ABx(c, Op_uint_imm, reg, cast(u32)u);
    } else {
        TValue tv = tvalue_make_uint(u);
        u32    k  = chunk_add_constant(c->L, c->chunk, tv);
        compiler_code_ABx(c, Op_uint_k, reg, k);
    }
}

static void
compiler_load_int(Compiler *c, u16 reg, lulu_int i)
{
    if (ARG_sBx_MIN <= i && i <= ARG_sBx_MAX) {
        compiler_code_AsBx(c, Op_int_imm, reg, cast(i32)i);
    } else {
        TValue tv = tvalue_make_int(i);
        u32    k  = chunk_add_constant(c->L, c->chunk, tv);
        compiler_code_ABx(c, Op_int_k, reg, k);
    }
}

static void
compiler_load_real(Compiler *c, u16 reg, lulu_real r)
{
    TValue tv = tvalue_make_real(r);
    u32    k  = chunk_add_constant(c->L, c->chunk, tv);
    compiler_code_ABx(c, Op_real, reg, k);
}

/*
 Description:
    Emits the code needed to retrieve the expression from the given register.
    This is 'discharging', indicating the expression has a finalized
    register it can refer to from here on.

 Analog:
    Lua 5.1.5: `lcode.c:discharge2reg (FuncState *fs, expdesc *e, int reg)`
 */
static void
expr_discharge_reg(Compiler *c, Expr *e, u16 reg)
{
    expr_discharge_vars(c, e);
    switch (e->kind) {
    case Expr_Literal: 
        switch (expr_literal_kind(e)) {
        case Value_bool: compiler_load_bool(c, reg, e->literal_bool, /*skip=*/false); break;
        case Value_uint: compiler_load_uint(c, reg, e->literal_uint); break;
        case Value_int:  compiler_load_int (c, reg, e->literal_int);  break;
        case Value_real: compiler_load_real(c, reg, e->literal_real); break;
        default:
            compiler_error(c, "Unsupported literal", e);
            break;
        }
        break;
    case Expr_Discharged:
        if (reg != e->reg) {
            compiler_code_ABC(c, Op_move, reg, e->reg, 0);
        }
        break;
    case Expr_Compare:
        compiler_load_bool(c, reg, true,  /*skip=*/true);
        compiler_load_bool(c, reg, false, /*skip=*/false);
        break;
    case Expr_Pending: {
        Instruction *ip = &c->chunk->code[expr_pc(e)];
        SETARG_A(ip, reg);
        break;
    }
    default:
        LULU_ASSERT(!e->kind);
        break;
    }

    e->kind = Expr_Discharged;
    e->reg  = reg;
}

static u16
expr_to_reg(Compiler *c, Expr *e, u16 reg)
{
    expr_discharge_reg(c, e, reg);
    e->kind = Expr_Discharged;
    e->reg  = reg;
    return reg;
}


LULU_INTERNAL_FUNC u16
compiler_expr_next_reg(Compiler *c, Expr *e)
{
    expr_discharge_vars(c, e);
    expr_pop(c, e);
    reg_push(c, 1);
    return expr_to_reg(c, e, c->free_reg - 1);
}

LULU_INTERNAL_FUNC u16
compiler_expr_any_reg(Compiler *c, Expr *e)
{
    expr_discharge_vars(c, e);
    // Already have a register?
    if (e->kind == Expr_Discharged) {
        // TODO(2026-07-13): Handle jumps
        return expr_reg(e);
    }
    return compiler_expr_next_reg(c, e);
}

static bool
compiler_unary_neg(Compiler *c, Expr *e)
{
    if (expr_is_literal(e)) switch (expr_literal_kind(e)) {
    case Value_uint:
        // TODO(2026-07-11): Do we want to check for overflow?
        e->type        = basic_type_get(Value_int);
        e->literal_int = -cast(lulu_int)e->literal_uint;
        return true;
    case Value_int:  e->literal_int  = -e->literal_int;  return true;
    case Value_real: e->literal_real = -e->literal_real; return true;
    default:
        return false;
    }

    compiler_expr_any_reg(c, e);
    if (expr_has_basic_type(e)) {
        u16 reg = expr_reg(e);
        switch (expr_basic_kind(e)) {
        case Value_uint:
        case Value_int:
            e->pc   = compiler_code_ABC(c, Op_neg, INVALID_REG, reg, 0);
            e->kind = Expr_Pending;
            return true;
        case Value_real:
            e->pc   = compiler_code_ABC(c, Op_fneg, INVALID_REG, reg, 0);
            e->kind = Expr_Pending;
            return true;
        default: break;
        }
    }
    return false;
}

static bool
compiler_unary_not(Compiler *c, Expr *e)
{
    switch (e->kind) {
    case Expr_Literal:
        if (expr_has_basic_kind(e, Value_bool)) {
            e->literal_bool = !e->literal_bool;
            return true;
        }
        break;
    case Expr_Compare: {
        Instruction *ip = &c->chunk->code[expr_compare(e)];
        bool const   k  = GETARG_k(*ip);
        SETARG_k(ip, !k);
        return true;
    }
    default:
        compiler_expr_any_reg(c, e);
        if (expr_has_basic_kind(e, Value_bool)) {
            u16 reg = expr_reg(e);
            e->pc   = compiler_code_ABC(c, Op_not, INVALID_REG, reg, 0);
            e->kind = Expr_Pending;
            return true;
        }
        break;
    }
    return false;
}

static bool
compiler_unary_op(Compiler *c, Token const *op, Expr *e)
{
    switch (op->kind) {
    case Token_Sub: return compiler_unary_neg(c, e);
    case Token_not: return compiler_unary_not(c, e);
    default:
        break;
    }
    LULU_UNREACHABLE();
    return false;
}

LULU_INTERNAL_FUNC void
compiler_unary(Compiler *c, Token const *op, Expr *e)
{
    if (!compiler_unary_op(c, op, e)) {
        compiler_error(c, "Invalid unary operand", e);
    }
}

static bool
compiler_coerce_signs(Compiler *c, Expr *u, Expr *i)
{
    // All positive signed integers can be coerced to unsigned integers.
    lulu_int i_imm = expr_int(i);
    if (i_imm >= 0) {
        i->literal_uint = cast(lulu_uint)i_imm;
        i->type         = u->type;
    }
    // Otherwise, try to coerce the unsigned integer to a signed one.
    else {
        lulu_uint u_imm = expr_uint(u);
        // Can't possibly be represented as a signed integer.
        if (u_imm > LULU_INT_MAX) {
            compiler_error(c, "Implicit cast to signed would overflow", u);
            return false;
        }
        u->literal_int = cast(lulu_int)u_imm;
        u->type        = i->type;
    }
    return true;
}

#define compiler_coerce_real(T, i, r) \
    ((i)->literal_real = cast(lulu_real)expr_##T(i), \
     (i)->type         = (r)->type,                      \
     true)

// TODO(2026-07-13): Make work with variables paired with literals!
static bool
compiler_implicit_cast_numeric(Compiler *c, Expr *lhs, Expr *rhs)
{
    ValueKind lhs_kind, rhs_kind;

    lhs_kind = expr_literal_kind(lhs);
    rhs_kind = expr_literal_kind(rhs);
    switch (lhs_kind) {
    case Value_uint:
        switch (rhs_kind) {
        case Value_uint: LULU_UNREACHABLE(); break;
        case Value_int:  return compiler_coerce_signs(c, lhs, rhs);
        case Value_real: return compiler_coerce_real (uint, lhs, rhs);
        default:
            break;
        }
        break;
    case Value_int:
        switch (rhs_kind) {
        case Value_uint: return compiler_coerce_signs(c, rhs, lhs);
        case Value_int:  LULU_UNREACHABLE(); break;
        case Value_real: return compiler_coerce_real(int, lhs, rhs);
        default:
            break;
        }
        break;
    case Value_real:
        // TODO(2026-07-08): Do we really want propagation?
        switch (rhs_kind) {
        case Value_uint: return compiler_coerce_real(uint, rhs, lhs);
        case Value_int:  return compiler_coerce_real(int,  rhs, lhs);
        case Value_real: LULU_UNREACHABLE(); break;
        default:
            break;
        }
        break;
    default:
        return false;
    }
    return false;
}

#undef compiler_coerce_real

// I'm not even going to pretend any of this is remotely sane
#define arg(e, T)      (e)->literal_##T
#define arith(T, op)   arg(lhs, T) op arg(rhs, T)
#define div(T, op)     if (arg(rhs, T) == 0) {return false;} arith(T, op)
#define mod            div
#define compare(T, op) expr_set_bool_literal(lhs, arg(lhs, T) op arg(rhs, T))
#define binary(T)                                                              \
    switch ((op)->kind) {                                                      \
    case Token_Add: arith  (T, +=); break;                                     \
    case Token_Sub: arith  (T, -=); break;                                     \
    case Token_Mul: arith  (T, *=); break;                                     \
    case Token_Div: div    (T, /=); break;                                     \
    case Token_Mod: mod    (T, %=); break;                                     \
    case Token_Eq:  compare(T, ==); break;                                     \
    case Token_Neq: compare(T, !=); break;                                     \
    case Token_Lt:  compare(T, <);  break;                                     \
    case Token_Leq: compare(T, <=); break;                                     \
    case Token_Gt:  compare(T, >);  break;                                     \
    case Token_Geq: compare(T, >=); break;                                     \
    default:                                                                   \
        LULU_UNREACHABLE();                                                    \
        break;                                                                 \
    }

static bool
compiler_implicit_cast_rhs(Compiler *c, Type const *type, Expr *rhs);

static bool
compiler_binary_folded(Compiler *c, Token const *op, Expr *lhs, Expr *rhs)
{
    if (!expr2_both_literal(lhs, rhs)) {
        if (expr_is_literal(lhs)) {
            Expr *tmp = lhs;
            lhs = rhs;
            rhs = tmp;
        }
        compiler_implicit_cast_rhs(c, lhs->type, rhs);
        return false;
    } else if (lhs->type != rhs->type) {
        if (!compiler_implicit_cast_numeric(c, lhs, rhs)) {
            return false;
        }
    }

#if !PARSER_CONSTANT_FOLDING
    return false;
#endif

    switch (expr_literal_kind(lhs)) {
    case Value_bool: {
        bool lhs_bool = lhs->literal_bool;
        bool rhs_bool = rhs->literal_bool;
        switch (op->kind) {
        case Token_Eq:  lhs->literal_bool = (lhs_bool == rhs_bool); break;
        case Token_Neq: lhs->literal_bool = (lhs_bool != rhs_bool); break;
        case Token_and: lhs->literal_bool = (lhs_bool && rhs_bool); break;
        case Token_or:  lhs->literal_bool = (lhs_bool || rhs_bool); break;
        default: return false;
        }
        break;
    }
    case Value_uint: binary(uint); break;
    case Value_int:  binary(int);  break;
#undef mod
#define mod(...) return false
    case Value_real: binary(real); break;
    default:
        return false;
    }
    return true;
}

#undef mod
#undef div
#undef binary
#undef compare
#undef arith
#undef arg

#define FLAG_COMPARE    (1 << 0)
#define FLAG_SWITCH     (1 << 1)
#define FLAG_NOT        (1 << 2)

/*
Description:
    The simplest type-checker possible!
    
Notes
    If constant folding is disabled, this will fail for trivial expressions
    like `cast(real)1 / 4`, but I'm willing to make that a problem for the
    constant folder.
*/
static OpCode
compiler_binary_op(Compiler *c, Token const *op, Expr *lhs, Expr const *rhs, u8 *flags)
{
    if (lhs->type != rhs->type) {
        compiler_error(c, "Inconsistent right hand side type", rhs);
    }

    // Neither lhs nor rhs are necessarily a literal by this point!
    if (type_is_basic(lhs->type)) switch (expr_basic_kind(lhs)) {
    case Value_bool:
        break;
    case Value_uint:
        switch (op->kind) {
        case Token_Add: return Op_add;
        case Token_Sub: return Op_sub;
        case Token_Mul: return Op_mul;
        case Token_Div: return Op_div;
        case Token_Mod: return Op_mod;
        case Token_Neq: *flags |= FLAG_NOT;
        case Token_Eq:  *flags |= FLAG_COMPARE;
                        return Op_eq;
        case Token_Gt:  *flags |= FLAG_SWITCH;
        case Token_Lt:  *flags |= FLAG_COMPARE;
                        return Op_lt;
        case Token_Geq: *flags |= FLAG_SWITCH;
        case Token_Leq: *flags |= FLAG_COMPARE;
                        return Op_leq;
        default:
            break;
        }
    case Value_int:
        switch (op->kind) {
        case Token_Add: return Op_add;
        case Token_Sub: return Op_sub;
        case Token_Mul: return Op_imul;
        case Token_Div: return Op_idiv;
        case Token_Mod: return Op_imod;
        case Token_Neq: *flags |= FLAG_NOT;
        case Token_Eq:  *flags |= FLAG_COMPARE;
                        return Op_eq;
        case Token_Gt:  *flags |= FLAG_SWITCH;
        case Token_Lt:  *flags |= FLAG_COMPARE;
                        return Op_ilt;
        case Token_Geq: *flags |= FLAG_SWITCH;
        case Token_Leq: *flags |= FLAG_COMPARE;
                        return Op_ileq;
        default:
            break;
        }
    case Value_real:
        switch (op->kind) {
        case Token_Add: return Op_fadd;
        case Token_Sub: return Op_fsub;
        case Token_Mul: return Op_fmul;
        case Token_Div: return Op_fdiv;
        case Token_Mod:
            compiler_error(c, "Modulo disallowed for reals- use 'fmod()'.", lhs);
            break;
        case Token_Neq: *flags |= FLAG_NOT;
        case Token_Eq:  *flags |= FLAG_COMPARE;
                        return Op_feq;
        case Token_Gt:  *flags |= FLAG_SWITCH;
        case Token_Lt:  *flags |= FLAG_COMPARE;
                        return Op_flt;
        case Token_Geq: *flags |= FLAG_SWITCH;
        case Token_Leq: *flags |= FLAG_COMPARE;
                        return Op_fleq;
        default:
            break;
        }
    default:
        break;
    }

    compiler_error(c, "Invalid left hand side operand", lhs);
    return Op_None;
}

LULU_INTERNAL_FUNC void
compiler_binary(Compiler *c, Token const *op, Expr *lhs, Expr *rhs)
{
    if (compiler_binary_folded(c, op, lhs, rhs)) {
        return;
    }

    u8     flags  = 0;
    OpCode opcode = compiler_binary_op(c, op, lhs, rhs, &flags);
    u16    r1     = expr_reg(lhs);
    u16    r2     = compiler_expr_any_reg(c, rhs);
    if (r1 > r2) {
        expr_pop(c, lhs);
        expr_pop(c, rhs);
    } else {
        expr_pop(c, rhs);
        expr_pop(c, lhs);
    }

    if (flags & FLAG_SWITCH) {
        LULU_ASSERT(flags & FLAG_COMPARE);
        u16 tmp = r1;
        r1 = r2;
        r2 = tmp;
    }

    if (flags & FLAG_COMPARE) {
        bool k     = (flags & FLAG_NOT) == 0;
        lhs->type  = basic_type_get(Value_bool);
        lhs->token = rhs->token;
        lhs->pc    = compiler_code_vABC(c, opcode, 0, r1, r2, k);
        lhs->kind  = Expr_Compare;
    } else {
        lhs->token = rhs->token;
        lhs->pc    = compiler_code_ABC(c, opcode, INVALID_REG, r1, r2);
        lhs->kind  = Expr_Pending;
    }
}

#undef FLAG_NOT
#undef FLAG_SWITCH
#undef FLAG_COMPARE

LULU_INTERNAL_FUNC void
compiler_return(Compiler *c, Expr *e)
{
    u16 reg = (e) ? compiler_expr_any_reg(c, e) : 0;
    compiler_code_ABC(c, Op_return, reg, 1, 1);
}

static bool
compiler_implicit_uint_from_int(Expr *rhs)
{
    lulu_int i = expr_int(rhs);
    // Can't implicitly cast negatives to unsigned.
    if (i < 0) {
        return false;
    }
    expr_set_uint_literal(rhs, cast(lulu_uint)i);
    return true;
}

static bool
compiler_implicit_uint_from_real(Expr *rhs)
{
    lulu_real r = expr_real(rhs);
    lulu_uint u = cast(lulu_uint)r;
    if (r < 0.0 || cast(lulu_real)u != r) {
        return false;
    }
    expr_set_uint_literal(rhs, u);
    return true;
}

static bool
compiler_implicit_int_from_uint(Expr *rhs)
{
    lulu_uint u = expr_uint(rhs);
    if (u > LULU_INT_MAX) {
        return false;
    }
    expr_set_int_literal(rhs, cast(lulu_int)u);
    return true;
}

static bool
compiler_implicit_int_from_real(Expr *rhs)
{
    lulu_real r = expr_real(rhs);
    lulu_int  i = cast(lulu_int)r;
    if (cast(lulu_real)i != r) {
        return false;
    }
    expr_set_int_literal(rhs, i);
    return true;
}

static bool
compiler_implicit_real_from_uint(Expr *rhs)
{
    lulu_uint u = expr_uint(rhs);
    lulu_real r = cast(lulu_real)u;

    // Conversion results in data loss?
    if (cast(lulu_uint)r != u) {
        return false;
    }
    expr_set_real_literal(rhs, r);
    return true;
}

static bool
compiler_implicit_real_from_int(Expr *rhs)
{
    lulu_int  i = expr_int(rhs);
    lulu_real r = cast(lulu_real)i;

    // Conversion results in data loss?
    if (cast(lulu_int)r != i) {
        return false;
    }

    expr_set_real_literal(rhs, r);
    return true;
}

#define compiler_unreachable_bool(rhs)      (LULU_UNREACHABLE(), false)
#define compiler_implicit_uint_from_uint    compiler_unreachable_bool
#define compiler_implicit_int_from_int      compiler_unreachable_bool
#define compiler_implicit_real_from_real    compiler_unreachable_bool
#define compiler_implicit_cast(rhs, T)                                         \
    switch ((rhs)->type->basic.kind) {                                         \
    case Value_uint: return compiler_implicit_##T##_from_uint(rhs);            \
    case Value_int:  return compiler_implicit_##T##_from_int (rhs);            \
    case Value_real: return compiler_implicit_##T##_from_real(rhs);            \
    default:         break;                                                    \
    }

static bool
compiler_implicit_cast_rhs(Compiler *c, Type const *type, Expr *rhs)
{
    // If rhs isn't a literal and it's of the wrong type, then we don't
    // allow it to assign to lhs as implicit casts are error-prone.
    if (!type_is_basic(type) || !expr_is_literal(rhs)) {
        return false;
    }

    switch (type->basic.kind) {
    case Value_uint: compiler_implicit_cast(rhs, uint); break;
    case Value_int:  compiler_implicit_cast(rhs, int);  break;
    case Value_real: compiler_implicit_cast(rhs, real); break;
    default:         break;
    }
    return false;
}

#undef compiler_implicit_cast
#undef compiler_implicit_real_from_real
#undef compiler_implicit_int_from_int
#undef compiler_implicit_uint_from_uint
#undef compiler_unreachable_bool

LULU_INTERNAL_FUNC void
compiler_declare(Compiler *c, Expr *lhs, Expr *rhs)
{
    // No assigning expression given, so use the zero value.
    if (!rhs->kind) {
        switch (lhs->type->kind) {
        case TypeKind_Basic:
            switch (lhs->type->basic.kind) {
            case Value_bool: expr_set_bool_literal(rhs, false); break;
            case Value_uint: expr_set_uint_literal(rhs, 0);     break;
            case Value_int:  expr_set_int_literal (rhs, 0);     break;
            case Value_real: expr_set_real_literal(rhs, 0.0);   break;
            default:
                goto nodice;
            }
            break;
        default: nodice:
            compiler_error(c, "Unsupported zero value", rhs);
        }
    }
    // Otherwise, we have an assigning expression but it's of the wrong type.
    else if (lhs->type != rhs->type) {
        // Only literals that can be implicitly converted to the destination
        // type without any loss of data will pass this check.
        if (!compiler_implicit_cast_rhs(c, lhs->type, rhs)) {
            compiler_error(c, "Invalid implicit cast", rhs);
        }
    }

    // Ensure register allocation is correct- locals variables are just
    // registers, after all.
    u16 const reg = compiler_expr_any_reg(c, rhs);
    LULU_ASSERTF(reg == c->active_count, "Expected active_count = %u but got %u", c->active_count, reg);

    Variable *v = &c->variables[c->active_count++];
    v->type     = lhs->type;
    v->name     = lhs->token.lexeme;
    v->scope    = 1;
    lhs->reg    = reg;
}

#undef compiler_code_ABx
#undef compiler_code_ABC
