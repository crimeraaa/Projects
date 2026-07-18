#include <math.h> // trunc

#include "lulu.h"
#include "internal.hpp"
#include "mem.hpp"
#include "value.hpp"
#include "opcode.hpp"
#include "type.hpp"
#include "expr.hpp"
#include "parser.hpp"
#include "chunk.hpp"
#include "compiler.hpp"

LULU_INTERNAL_FUNC void
compiler_finish(Compiler *c)
{
    lulu_State *L     = c->L;
    Chunk *     chunk = c->chunk;
    compiler_return(c, nullptr);
    // Shrink chunk to fit.
    chunk->code       = mem_heap_resize(L, chunk->code, chunk->code_cap, c->pc);
    chunk->code_cap   = c->pc;
    chunk->values     = mem_heap_resize(L, chunk->values, chunk->values_cap, c->constants_count);
    chunk->values_cap = c->constants_count;
}

[[noreturn]] static void
compiler_error(Compiler *c, char const *info, Expr const *e)
{
    parser_error_at(c->parser, info, e->token);
}

static i32
compiler_code(Compiler *c, Instruction i)
{
    Chunk *chunk = c->chunk;
    i32    idx   = c->pc++;
    if (c->pc > chunk->code_cap) {
        chunk->code = mem_heap_grow(c->L, chunk->code, &chunk->code_cap);
    }
    chunk->code[idx] = i;
    return idx;
}

static u32
compiler_add_constant(Compiler *c, TValue tv)
{
    Chunk * chunk = c->chunk;
    TValue *K     = chunk->values;
    u32     n     = c->constants_count;

    // Try to reuse an existing value.
    for (u32 i = 0; i < n; i++) {
        if (tvalue_eq(tv, K[i])) {
            return i;
        }
    }

    // Definitely need to append this value to the array.
    c->constants_count += 1;
    if (c->constants_count > chunk->values_cap) {
        chunk->values = mem_heap_grow(c->L, chunk->values, &chunk->values_cap);
    }
    chunk->values[n] = tv;
    return n;
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
compiler_code_vABC(Compiler *c, OpCode Op, u16 A, u16 B, u16 C, bool k)
{
    LULU_ASSERT(opcode_is_vABC(Op));
    LULU_ASSERT(A <= ARG_A_MAX);
    LULU_ASSERT(B <= ARG_B_MAX);
    LULU_ASSERT(C <= ARG_vC_MAX);
    return compiler_code(c, MAKE_vABC(Op, A, B, C, cast(u8)k));
}

static i32
compiler_code_ABx(Compiler *c, OpCode Op, u16 A, u32 Bx)
{
    LULU_ASSERT(opcode_is_ABx(Op));
    LULU_ASSERT(A  <= ARG_A_MAX);
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

static i32
compiler_code_vAsBx(Compiler *c, OpCode Op, u16 A, i32 vsBx, bool k)
{
    LULU_ASSERT(opcode_is_vAsBx(Op));
    LULU_ASSERT(A <= ARG_A_MAX);
    LULU_ASSERT(ARG_vsBx_MIN <= vsBx && vsBx <= ARG_vBx_MAX);
    return compiler_code(c, MAKE_AvsBx(Op, A, vsBx, k));
}

static bool
compiler_cast_bool(Compiler *c, Expr *e)
{
    u16    reg = expr_reg(e);
    OpCode op;
    switch (expr_basic_kind(e)) {
    case Value_bool: LULU_UNREACHABLE(); return false;
    case Value_int:  op = Op_eqi;  break;
    case Value_real: op = Op_feqi; break;
    default:         return false;
    }
    e->pc   = compiler_code_vAsBx(c, op, reg, 0, true);
    e->kind = Expr_Compare;
    return true;
}

static bool
compiler_cast_int(Compiler *c, Expr *e)
{
    u16 reg = expr_reg(e);
    switch (expr_basic_kind(e)) {
    case Value_bool: LULU_PANICLN("cast(int) not yet implemented for 'bool'"); break;
    case Value_int:  LULU_UNREACHABLE(); break;
    case Value_real:
        e->pc   = compiler_code_ABC(c, Op_real2int, NO_REG, reg, 0);
        e->kind = Expr_Pending;
        return true;
    default:
        break;
    }
    return false;
}

static bool
compiler_cast_real(Compiler *c, Expr *e)
{
    u16 reg = expr_reg(e);
    switch (expr_basic_kind(e)) {
    case Value_bool: LULU_PANICLN("cast(real) not yet implemented for 'bool'"); break;
    case Value_int:
        e->pc   = compiler_code_ABC(c, Op_int2real, NO_REG, reg, 0);
        e->kind = Expr_Pending;
        return true;
    case Value_real: LULU_UNREACHABLE(); break;
    default:         break;
    }
    return false;
}

static bool
compiler_cast_basic_type(Compiler *c, Expr *e, ValueKind dst_kind)
{
    LULU_ASSERT(e->type != nullptr);
    switch (e->kind) {
    case Expr_Literal:
        switch (expr_literal_kind(e)) {
        case Value_bool: {
            bool b = expr_bool(e);
            switch (dst_kind) {
            case Value_int:  expr_set_int (e, cast(lulu_int) b); break;
            case Value_real: expr_set_real(e, cast(lulu_real)b); break;
	        default:
                LULU_UNREACHABLE();
                return false;
            }
        }
        case Value_int: {
            lulu_int i = expr_int(e);
            switch (dst_kind) {
            case Value_bool: expr_set_bool(e, cast(bool)i);      break;
            case Value_real: expr_set_real(e, cast(lulu_real)i); break;
	        default:
                LULU_UNREACHABLE();
                return false;
            }
        }
        case Value_real: {
            lulu_real r = expr_real(e);
            switch (dst_kind) {
            case Value_bool: expr_set_bool(e, cast(bool)r);     break;
            case Value_int:  expr_set_int (e, cast(lulu_int)r); break;
            default:
                LULU_UNREACHABLE();
                return false;
            }
            break;
        }
        default:
            LULU_UNREACHABLE();
            return false;
        }
        break;
    case Expr_Variable: [[fallthrough]];
    case Expr_Pending: compiler_expr_any_reg(c, e); [[fallthrough]];
    case Expr_Discharged:
        switch (dst_kind) {
        case Value_bool: if (!compiler_cast_bool(c, e)) { goto nodice; } break;
        case Value_int:  if (!compiler_cast_int (c, e)) { goto nodice; } break;
        case Value_real: if (!compiler_cast_real(c, e)) { goto nodice; } break;
        default:
            LULU_UNREACHABLE();
            break;
        }
        e->type = basic_type_get(dst_kind);
        break;
    default: nodice:
        LULU_PANICF("Got ExprKind(%i)", e->kind);
        compiler_error(c, "Cannot cast to type", e);
        return false;
    }
    return true;
}

LULU_INTERNAL_FUNC void
compiler_cast(Compiler *c, Type const *t, Expr *e)
{   
    // CHECK(2026-07-16): literal is typed or untyped
    LULU_ASSERT(e->type != nullptr);

    // Nothing to do?
    if (e->type == t) {
        return;
    }

    switch (t->kind) {
    case TypeKind_Basic:
        switch (t->basic.kind) {
        case Value_bool: [[fallthrough]];
        case Value_int:  [[fallthrough]];
        case Value_real:
            if (compiler_cast_basic_type(c, e, t->basic.kind)) {
                return;
            }
        default:
            break;
        }
        break;
    default:
        // Need dedicated instructions!
        break;
    }
    LULU_UNIMPLEMENTED();
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
compiler_load_int(Compiler *c, u16 reg, lulu_int i)
{
    if (ARG_sBx_MIN <= i && i <= ARG_sBx_MAX) {
        compiler_code_AsBx(c, Op_int_imm, reg, cast(i32)i);
    } else {
        TValue tv = tvalue_make_int(i);
        u32    k  = compiler_add_constant(c, tv);
        compiler_code_ABx(c, Op_int_k, reg, k);
    }
}

static void
compiler_load_real(Compiler *c, u16 reg, lulu_real r)
{
    TValue tv = tvalue_make_real(r);
    u32    k  = compiler_add_constant(c, tv);
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
        // CHECK(2026-07-16): Literal is typed or untyped
        LULU_ASSERT(e->type != nullptr);
        switch (expr_literal_kind(e)) {
        case Value_bool: compiler_load_bool(c, reg, expr_bool(e), /*skip=*/false); break;
        case Value_int:  compiler_load_int (c, reg, expr_int (e)); break;
        case Value_real: compiler_load_real(c, reg, expr_real(e)); break;
        default:
            compiler_error(c, "Unsupported literal", e);
            break;
        }
        break;
    case Expr_Discharged:
        // Moving to the same register is a no-op, e.g. `local x := 1; x = x;`.
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
    if (expr_is_literal(e)) {
        return expr_neg(e);
    }

    u16 reg = compiler_expr_any_reg(c, e);
    OpCode op;
    if (expr_has_basic_type(e)) switch (expr_basic_kind(e)) {
    case Value_int:  op = Op_neg;  break;
    case Value_real: op = Op_fneg; break;
    default:         return false;
    }
    e->pc   = compiler_code_ABC(c, op, NO_REG, reg, 0);
    e->kind = Expr_Pending;
    return true;
}

static bool
compiler_unary_not(Compiler *c, Expr *e)
{
    switch (e->kind) {
    case Expr_Literal:
        // Only boolean literals can have `not` applied to them.
        if (!expr_has_basic_kind(e, Value_bool)) {
            return false;
        }
        value_set_bool(&e->literal, !value_bool(e->literal));
        return true;
    case Expr_Compare: {
        // E.g. `not (x == y)`
        Instruction *ip = &c->chunk->code[expr_compare(e)];
        bool const   k  = GETARG_k(*ip);
        SETARG_k(ip, !k);
        return true;
    }
    default:
        break;
    }

    u16 reg = compiler_expr_any_reg(c, e);
    if (expr_has_basic_kind(e, Value_bool)) {
        e->pc   = compiler_code_ABC(c, Op_not, NO_REG, reg, 0);
        e->kind = Expr_Pending;
        return true;
    }
    return false;
}

static bool
compiler_unary_dispatch(Compiler *c, Token const &op, Expr *e)
{
    switch (op.kind) {
    case Token_Sub: return compiler_unary_neg(c, e);
    case Token_not: return compiler_unary_not(c, e);
    default:
        break;
    }
    LULU_UNREACHABLE();
    return false;
}

LULU_INTERNAL_FUNC void
compiler_unary(Compiler *c, Token const &op, Expr *e)
{
    if (!compiler_unary_dispatch(c, op, e)) {
        compiler_error(c, "Invalid unary operand", e);
    }
}

static bool
compiler_coerce_rhs(Type const *type, Expr *rhs)
{
    // If rhs isn't a literal and it's of the wrong type, then we don't
    // allow it to assign to lhs as implicit casts are error-prone.
    if (!type_is_basic(type) || !expr_is_literal(rhs)) {
        return false;
    }

    switch (type->basic.kind) {
    // E.g. `x: int = 1.0` should succeed, but `x: int = 1.2` should fail.
    case Value_int:
        if (rhs->literal_kind == Value_real) {
            return expr_coerce_safe<lulu_int, lulu_real>(rhs);
        }
        break;

    // E.g. `x: real = 1` should succeed,
    // but  `x: real = 90_071_992_454_740_993` should fail.
    case Value_real:
        if (rhs->literal_kind == Value_int) {
            return expr_coerce_safe<lulu_real, lulu_int>(rhs);
        }
        break;
    default:
        break;
    }
    return false;
}


// TODO(2026-07-13): Make work with variables paired with literals!
static bool
compiler_coerce_numeric(Compiler *c, Expr *restrict lhs, Expr *restrict rhs)
{
    ValueKind rhs_kind = expr_literal_kind(rhs);
    switch (expr_literal_kind(lhs)) {
    case Value_int:
        switch (rhs_kind) {
        case Value_int:  LULU_UNREACHABLE(); break;
        case Value_real: return expr_coerce_safe<lulu_int, lulu_real>(rhs);
        default:
            break;
        }
        break;
    case Value_real:
        // TODO(2026-07-08): Do we really want propagation?
        switch (rhs_kind) {
        case Value_int:  return expr_coerce_safe<lulu_real, lulu_int>(rhs);
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

template<class T>
static inline void
expr_divmod(Compiler *c, T (*op)(T a, T b), Expr *restrict lhs, Expr *restrict rhs)
{
    auto lhs_literal = expr_literal<T>(lhs);
    auto rhs_literal = expr_literal<T>(rhs);
    if (rhs_literal == 0) {
        compiler_error(c, "Cannot divide/modulo by 0", rhs);
    }
    value_set<T>(&lhs->literal, (*op)(lhs_literal, rhs_literal));
}

template<class T>
static inline void
expr_arith(T (*op)(T a, T b), Expr *restrict lhs, Expr *restrict rhs)
{
    auto res = (*op)(expr_literal<T>(lhs), expr_literal<T>(rhs));
    value_set<T>(&lhs->literal, res);
}

template<class T>
static inline void
expr_comp(bool (*op)(T a, T b), Expr *restrict lhs, Expr *restrict rhs, bool flip)
{
    bool b = (*op)(expr_literal<T>(lhs), expr_literal<T>(rhs));
    value_set<T>(&lhs->literal, (flip) ? !b : b);
}

/*
 Assumptions:
 1) Both operands are literal values of the same underlying type.
 */
template<class T>
static void
compiler_binary_fold_literals(Compiler *c,
    Token const &  op,
    Expr *restrict lhs,
    Expr *restrict rhs)
{
    bool flip = false;
    switch (op.kind) {
    // Arithmetic
    case Token_Add: expr_arith <T>(num_add<T>, lhs, rhs);    return;
    case Token_Sub: expr_arith <T>(num_sub<T>, lhs, rhs);    return;
    case Token_Mul: expr_arith <T>(num_mul<T>, lhs, rhs);    return;
    case Token_Div: expr_divmod<T>(c, num_div<T>, lhs, rhs); return;
    case Token_Mod: expr_divmod<T>(c, num_mod<T>, lhs, rhs); return;

    // Comparison
    case Token_Neq: flip = true; [[fallthrough]];
    case Token_Eq:  expr_comp<T>(num_eq <T>, lhs, rhs, flip); return;
    case Token_Geq: flip = true; [[fallthrough]];
    case Token_Lt:  expr_comp<T>(num_lt <T>, lhs, rhs, flip); return;
    case Token_Gt:  flip = true; [[fallthrough]];
    case Token_Leq: expr_comp<T>(num_leq<T>, lhs, rhs, flip); return;
    default:
        break;
    }
}


static bool
compiler_binary_folded(Compiler *c,
    Token const &  op,
    Expr *restrict lhs,
    Expr *restrict rhs)
{
    if (!expr2_both_literal(lhs, rhs)) {
        return false;
    }

    // CHECK(2026-07-16): Literal is typed or untyped
    LULU_ASSERT(lhs->type != nullptr);
    LULU_ASSERT(rhs->type != nullptr);
    if (lhs->type != rhs->type) {
        if (!compiler_coerce_numeric(c, lhs, rhs)) {
            return false;
        }
    }

#if !PARSER_CONSTANT_FOLDING
    return false;
#endif

    switch (expr_literal_kind(lhs)) {
    case Value_bool: {
        bool a    = expr_bool(lhs);
        bool b    = expr_bool(rhs);
        bool flip = false;
        switch (op.kind) {
        case Token_Neq: flip = true; [[fallthrough]];
        case Token_Eq:  value_set(&lhs->literal, (flip) ? a != b : a == b); break;
        case Token_and: value_set(&lhs->literal, a && b); break;
        case Token_or:  value_set(&lhs->literal, a || b); break;
        default: return false;
        }
        break;
    }
    case Value_int:  compiler_binary_fold_literals<lulu_int> (c, op, lhs, rhs); break;
    case Value_real: compiler_binary_fold_literals<lulu_real>(c, op, lhs, rhs); break;
    default:         return false;
    }
    return true;
}

#define FLAG_COMPARE  (1 << 0)
#define FLAG_NOT      (1 << 1)

/*
Description:
    The simplest type-checker possible!
    
Notes
    If constant folding is disabled, this will fail for trivial expressions
    like `cast(real)1 / 4`, but I'm willing to make that a problem for the
    constant folder.
*/
static OpCode
compiler_binary_dispatch(Compiler *c,
    Token const &        op,
    Expr const *restrict lhs,
    Expr const *restrict rhs,
    u8 *        const    flags)
{
    if (lhs->type != rhs->type) {
        compiler_error(c, "Inconsistent right hand side type", rhs);
    }

    // Neither lhs nor rhs are necessarily a literal by this point!
    OpCode opcode;
    switch (op.kind) {
    case Token_Add: opcode = Op_add; break;
    case Token_Sub: opcode = Op_sub; break;
    case Token_Mul: opcode = Op_mul; break;
    case Token_Div: opcode = Op_div; break;
    case Token_Mod: opcode = Op_mod; break;
    case Token_Neq: *flags |= FLAG_NOT; [[fallthrough]];
    case Token_Eq:
        *flags |= FLAG_COMPARE;
        opcode = Op_eq;
        break;

    // x >= y <=> !(x < y)
    case Token_Geq: *flags |= FLAG_NOT; [[fallthrough]];
    case Token_Lt:
        *flags |= FLAG_COMPARE;
        opcode = Op_lt;
        break;
    // x > y <=> !(x <= y)
    case Token_Gt:  *flags |= FLAG_NOT; [[fallthrough]];
    case Token_Leq:
        *flags |= FLAG_COMPARE;
        opcode = Op_leq;
        break;
    default:
        break;
    }

    if (type_is_basic(lhs->type)) switch (expr_basic_kind(lhs)) {
    case Value_bool:
        // We don't allow ordered comparisons on booleans.
        if (opcode == Op_eq) {
            return opcode;
        }
        break;
    case Value_real: return opcode + (Op_fadd - Op_add);
    case Value_int:  return opcode;
    default:
        break;
    }

    compiler_error(c, "Invalid left hand side operand", lhs);
    return cast(OpCode)0;
}

static bool
compiler_arithi(Compiler *c, OpCode iop, Expr *restrict lhs, Expr *restrict rhs)
{
    Expr *dst = lhs;

    /*
     Consider the following forms:
     1)   imm  + y <=> y +   imm
     2) (-imm) + y <=> y + (-imm) <=> y - |imm|
     3    imm  - y <=> imm + (-y)
     4) (-imm) - y <=> -(|imm| + (-y))    <=> -((-y) + |imm|)

     1 can be safely inverted because addition is commutative.
     2 can be safely converted because we can decompose it into a subtraction.
     3 and 4 cannot be converted because they require a negation.
     */
    if (expr_is_literal(lhs)) {
        if (iop == Op_subi || iop == Op_fsubi) {
            return false;
        }
        swap(&lhs, &rhs);
    }

    /*
     Assumes any of the following forms:
     1) x +   imm
     2) x + (-imm) <=> x - |imm|
     3) x -   imm
     4) x - (-imm) <=> x + |imm|
     */
    u16      reg = expr_reg(lhs);
    lulu_int imm;
    if (!expr_int_safe(rhs, -cast(lulu_int)ARG_C_MAX, ARG_C_MAX, &imm)) {
        return false;
    }

    switch (iop) {
    case Op_addi:
    case Op_faddi:
        // Assumes that the corresponding sub opcode is 1 above us.
        if (imm < 0) {
            imm = -imm;
            iop = iop + 1;
        }
        break;
    case Op_subi:
    case Op_fsubi:
        // Assumes that the corresponding add opcode is 1 below us.
        if (imm < 0) {
            imm = -imm;
            iop = iop - 1;
        }
        break;
    default:
        LULU_UNREACHABLE();
        return false;
    }

    dst->pc   = compiler_code_ABC(c, iop, NO_REG, reg, cast(u16)imm);
    dst->kind = Expr_Pending;
    return true;
}

static bool
compiler_comparei(Compiler *c,
    OpCode         iop,
    Expr *restrict lhs,
    Expr *restrict rhs,
    bool           is_not)
{
    Expr *dst = lhs;
    if (expr_is_literal(lhs)) {
        swap(&lhs, &rhs);
    }

    u16 reg = expr_reg(lhs);
    if (expr_is_bool(rhs)) {
        if (is_not) {
            dst->pc   = compiler_code_ABC(c, Op_not, NO_REG, reg, 0);
            dst->kind = Expr_Pending;
        }
        return true;
    }

    lulu_int imm;
    if (!expr_int_safe(rhs, ARG_vsBx_MIN, ARG_vsBx_MAX, &imm)) {
        return false;
    }

    dst->pc = compiler_code_vAsBx(c, iop, reg, cast(i32)imm, is_not);
    return true;
}

/*
 Assumptions:
 1) Both arguments are of the same underlying type.

 2) The opcodes and types we support immediate operations for are commutative.
    if (!(-cast(lulu_int)ARG_C_MAX <= imm && imm <= ARG_C_MAX)) {
        return false;
    }
    I.e. `x op y` has the same effect as `y op x`, which is only true for some
    operations.
 */
static bool
compiler_binary_imm(Compiler *c,
    OpCode const    op,
    Expr * restrict lhs,
    Expr * restrict rhs,
    u8     const    flags)
{
    // If both are literals, then we should've folded them.
    // If they are both discharged or pending, we shouldn't have called this.
    LULU_ASSERT(expr_is_literal(lhs) != expr_is_literal(rhs));
    bool k = (flags & FLAG_NOT) == 0;
    switch (op) {
    case Op_add:  return compiler_arithi  (c, Op_addi,  lhs, rhs);
    case Op_sub:  return compiler_arithi  (c, Op_subi,  lhs, rhs);
    case Op_eq:   return compiler_comparei(c, Op_eqi,   lhs, rhs, k);
    case Op_lt:   return compiler_comparei(c, Op_lti,   lhs, rhs, k);
    case Op_leq:  return compiler_comparei(c, Op_leqi,  lhs, rhs, k);
    case Op_fadd: return compiler_arithi  (c, Op_faddi, lhs, rhs);
    case Op_fsub: return compiler_arithi  (c, Op_fsubi, lhs, rhs);
    case Op_feq:  return compiler_comparei(c, Op_feqi,  lhs, rhs, k);
    case Op_flt:  return compiler_comparei(c, Op_flti,  lhs, rhs, k);
    case Op_fleq: return compiler_comparei(c, Op_fleqi, lhs, rhs, k);
    default:      return false;
    }
}

LULU_INTERNAL_FUNC void
compiler_binary(Compiler *c, Token const &op, Expr *restrict lhs, Expr *restrict rhs)
{
    if (compiler_binary_folded(c, op, lhs, rhs)) {
        return;
    }

    // Ensure both arguments are of the same underyling type so that we
    // can dispatch the correct opcodes. Only literals can be coerced.
    if (expr_is_literal(lhs)) {
        // E.g. `1 + x` so we want to coerce `1` to the type of `x`.
        compiler_coerce_rhs(rhs->type, lhs);
    } else if (expr_is_literal(rhs)) {
        compiler_coerce_rhs(lhs->type, rhs);
    }

    u8     flags  = 0;
    OpCode opcode = compiler_binary_dispatch(c, op, lhs, rhs, &flags);

    // Try the immediate versions first.
    if (expr_is_literal(lhs) || expr_is_literal(rhs)) {
        if (compiler_binary_imm(c, opcode, lhs, rhs, flags)) {
            // Propagate this change because we won't do it any plac else.
            lhs->token = rhs->token;
            return;
        }
    }

    u16 r1 = expr_reg(lhs);
    u16 r2 = compiler_expr_any_reg(c, rhs);
    if (r1 > r2) {
        expr_pop(c, lhs);
        expr_pop(c, rhs);
    } else {
        expr_pop(c, rhs);
        expr_pop(c, lhs);
    }

    if (flags & FLAG_COMPARE) {
        /*
            ALlows us to implement complements of the 3 basic comparison
            instructions. This is useful for both assignments and
            conditional blocks.

            0 = proceed label(true) if not result else goto label(false)
            1 = proceed label(true) if     result else goto label(false)
         */
        bool k     = (flags & FLAG_NOT) == 0;
        lhs->type  = basic_type_get(Value_bool);
        lhs->token = rhs->token;

        // R(A) is not a destination register here!
        lhs->pc    = compiler_code_vABC(c, opcode, r1, r2, 0, k);
        lhs->kind  = Expr_Compare;
    } else {
        lhs->token = rhs->token;
        lhs->pc    = compiler_code_ABC(c, opcode, NO_REG, r1, r2);
        lhs->kind  = Expr_Pending;
    }
}

#undef FLAG_NOT
#undef FLAG_COMPARE

LULU_INTERNAL_FUNC void
compiler_return(Compiler *c, Expr *e)
{
    u16 reg = (e) ? compiler_expr_any_reg(c, e) : 0;
    compiler_code_ABC(c, Op_return, reg, 0, 0);
}

LULU_INTERNAL_FUNC void
compiler_declare(Compiler *c, Expr *restrict lhs, Expr *restrict rhs)
{
    Type const *type = lhs->type;
    // No assigning expression given, so use the zero value.
    if (!rhs->kind) {
        switch (type->kind) {
        case TypeKind_Basic:
            rhs->type = type;
            switch (type->basic.kind) {
            case Value_bool: value_set_bool(&rhs->literal, false); break;
            case Value_int:  value_set_int (&rhs->literal, 0);     break;
            case Value_real: value_set_real(&rhs->literal, 0.0);   break;
            default:
                goto nodice;
            }
            break;
        default: nodice:
            compiler_error(c, "Unsupported zero value", lhs);
        }
    }
    // Otherwise, we have an assigning expression but it's of the wrong type.
    else if (type != rhs->type) {
        // Only literals that can be implicitly converted to the destination
        // type without any loss of data will pass this check.
        if (!compiler_coerce_rhs(type, rhs)) {
            compiler_error(c, "Invalid implicit cast", rhs);
        }
    }

    // Ensure register allocation is correct- locals variables are just
    // registers, after all.
    u16 const reg = compiler_expr_next_reg(c, rhs);
    LULU_ASSERTF(reg == c->active_count, "Expected active_count = %u but got %u", c->active_count, reg);

    // Don't update the active count just yet so we can refer to the same
    // identifier in the assigning expression, e.g
    // ```
    // x := 1;
    // {
    //      x := x + 1;
    // }
    // ```
    Variable *v = &c->variables[c->active_count++];
    v->type     = type;
    v->name     = lhs->token.lexeme;
    v->scope    = -1;
    lhs->reg    = reg;
}

