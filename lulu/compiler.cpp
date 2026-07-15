#include <math.h> // trunc

#include "lulu.h"
#include "internal.hpp"
#include "value.hpp"
#include "opcode.hpp"
#include "type.hpp"
#include "expr.hpp"
#include "parser.hpp"
#include "chunk.hpp"
#include "compiler.hpp"

LULU_NORETURN static void
compiler_error(Compiler *c, char const *info, Expr const *e)
{
    parser_error_at(c->parser, info, e->token);
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
    return compiler_code(c, MAKE_ABC(Op, cast(u8)A, cast(u8)B, cast(u8)C));
}

static i32
compiler_code_vABC(Compiler *c, OpCode Op, u16 A, u16 B, u16 vC, bool k)
{
    LULU_ASSERT(opcode_is_vABC(Op));
    LULU_ASSERT(A  <= ARG_A_MAX);
    LULU_ASSERT(B  <= ARG_B_MAX);
    LULU_ASSERT(vC <= ARG_vC_MAX);
    return compiler_code(c, MAKE_vABC(Op, cast(u8)A, B, vC, k));
}

static i32
compiler_code_ABx(Compiler *c, OpCode Op, u16 A, u32 Bx)
{
    LULU_ASSERT(opcode_is_ABx(Op));
    LULU_ASSERT(A  <= ARG_A_MAX);
    LULU_ASSERT(Bx <= ARG_Bx_MAX);
    return compiler_code(c, MAKE_ABx(Op, cast(u8)A, Bx));
}

static i32
compiler_code_AsBx(Compiler *c, OpCode Op, u16 A, i32 sBx)
{
    LULU_ASSERT(opcode_is_AsBx(Op));
    LULU_ASSERT(A <= ARG_A_MAX);
    LULU_ASSERT(ARG_sBx_MIN <= sBx && sBx <= ARG_sBx_MAX);
    return compiler_code(c, MAKE_AsBx(Op, cast(u8)A, sBx));
}

// Cast the expression to T.
template<class T>
static bool
compiler_cast_basic_type(Compiler *c, Expr *e)
{
    auto constexpr dst_kind = trait_ValueKind<T>::kind;
    LULU_ASSERT(e->type != nullptr);
    switch (e->kind) {
    case Expr_Literal:
        // TODO(2026-07-14): Do something about dead code when types match?
        switch (e->literal_kind) {
        case Value_bool: expr_set<T>(e, cast(T)expr_bool(e)); break;
        case Value_int:  expr_set<T>(e, cast(T)expr_int (e)); break;
        case Value_real: expr_set<T>(e, cast(T)expr_real(e)); break;
        default:
            LULU_UNREACHABLE();
            break;
        }
        break;
    case Expr_Pending:
        compiler_expr_any_reg(c, e);
        [[fallthrough]];
    case Expr_Discharged: {
        // TODO(2026-07-16): Remove `Op_cast`, replace with explicit instructions
        u16 src_kind = cast(u16)expr_basic_kind(e);
        u16 reg      = expr_reg(e);
        compiler_code_ABC(c, Op_cast, reg, cast(u16)dst_kind, src_kind);
        e->type = basic_type_get(dst_kind);
        break;
    }
    default:
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
        case Value_bool: compiler_cast_basic_type<bool>     (c, e); return;
        case Value_int:  compiler_cast_basic_type<lulu_int> (c, e); return;
        case Value_real: compiler_cast_basic_type<lulu_real>(c, e); return;
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
    case Value_int:  value_set_int (&e->literal, -value_int (e->literal)); return true;
    case Value_real: value_set_real(&e->literal, -value_real(e->literal)); return true;
    default:
        return false;
    }

    u16 reg = compiler_expr_any_reg(c, e);
    if (expr_has_basic_type(e)) switch (expr_basic_kind(e)) {
    case Value_int:
        e->pc   = compiler_code_ABC(c, Op_neg, INVALID_REG, reg, 0);
        e->kind = Expr_Pending;
        return true;
    case Value_real:
        e->pc   = compiler_code_ABC(c, Op_fneg, INVALID_REG, reg, 0);
        e->kind = Expr_Pending;
        return true;
    default:
        break;
    }
    return false;
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
        e->pc   = compiler_code_ABC(c, Op_not, INVALID_REG, reg, 0);
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
compiler_coerce_real(Expr *i)
{
    auto arg = cast(lulu_real)expr_int(i);
    expr_set_real(i, arg);
    return true;
}

// TODO(2026-07-13): Make work with variables paired with literals!
static bool
compiler_coerce_numeric(Compiler *c, Expr *restrict lhs, Expr *restrict rhs)
{
    ValueKind lhs_kind, rhs_kind;

    lhs_kind = expr_literal_kind(lhs);
    rhs_kind = expr_literal_kind(rhs);
    switch (lhs_kind) {
    case Value_int:
        switch (rhs_kind) {
        case Value_int:  LULU_UNREACHABLE(); break;
        case Value_real: return compiler_coerce_real(lhs);
        default:
            break;
        }
        break;
    case Value_real:
        // TODO(2026-07-08): Do we really want propagation?
        switch (rhs_kind) {
        case Value_int:  return compiler_coerce_real(rhs);
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
        compiler_error(c, "Cannot divide by 0", rhs);
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

template<class Dst, class Src>
static bool
compiler_coerce_between(Expr *e)
{
    if (e->literal_kind != trait_ValueKind<Src>::kind) {
        return false;
    }

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
compiler_coerce_rhs(Type const *type, Expr *rhs)
{
    // If rhs isn't a literal and it's of the wrong type, then we don't
    // allow it to assign to lhs as implicit casts are error-prone.
    if (!type_is_basic(type) || !expr_is_literal(rhs)) {
        return false;
    }

    switch (type->basic.kind) {
    // E.g. `x: int = 1.0` should succeed, but `x: int = 1.2` should fail.
    case Value_int:  return compiler_coerce_between<lulu_int, lulu_real>(rhs);

    // E.g. `x: real = 1` should succeed,
    // but  `x: real = 90_071_992_454_740_993` should fail.
    case Value_real: return compiler_coerce_between<lulu_real, lulu_int>(rhs);
    default:         return false;
    }
}


static bool
compiler_binary_folded(Compiler *c,
    Token const &  op,
    Expr *restrict lhs,
    Expr *restrict rhs)
{
    if (!expr2_both_literal(lhs, rhs)) {
        // E.g. `1 + x` so we want to coerce `1` to the type of `x`.
        if (expr_is_literal(lhs)) {
            swap(&lhs, &rhs);
        }
        // Mutate the expressions in-place so that we can emit the proper
        // instructions even though we don't fold anything.
        compiler_coerce_rhs(lhs->type, rhs);
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
        case Token_Neq: flip = true;
                        [[fallthrough]];
        case Token_Eq:  value_set(&lhs->literal, (flip) ? a != b : a == b);
                        break;
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
compiler_binary_op(Compiler *c,
    Token const &            op,
    Expr *restrict           lhs,
    Expr const *restrict     rhs,
    u8 *const                flags)
{
    if (lhs->type != rhs->type) {
        compiler_error(c, "Inconsistent right hand side type", rhs);
    }

    // Neither lhs nor rhs are necessarily a literal by this point!
    if (type_is_basic(lhs->type)) switch (expr_basic_kind(lhs)) {
    case Value_int:
        switch (op.kind) {
        case Token_Add: return Op_add;
        case Token_Sub: return Op_sub;
        case Token_Mul: return Op_mul;
        case Token_Div: return Op_div;
        case Token_Mod: return Op_mod;
        case Token_Neq: *flags |= FLAG_NOT;
                        [[fallthrough]];
        case Token_Eq:  *flags |= FLAG_COMPARE;
                        return Op_eq;
        // x >= y <=> !(x < y)
        case Token_Geq: *flags |= FLAG_NOT;
                        [[fallthrough]];
        case Token_Lt:  *flags |= FLAG_COMPARE;
                        return Op_lt;
        // x > y <=> !(x <= y)
        case Token_Gt:  *flags |= FLAG_NOT;
                        [[fallthrough]];
        case Token_Leq: *flags |= FLAG_COMPARE;
                        return Op_leq;
        default:
            break;
        }
    case Value_real:
        switch (op.kind) {
        case Token_Add: return Op_fadd;
        case Token_Sub: return Op_fsub;
        case Token_Mul: return Op_fmul;
        case Token_Div: return Op_fdiv;
        case Token_Mod: return Op_fmod;
        case Token_Neq: *flags |= FLAG_NOT;
                        [[fallthrough]];
        case Token_Eq:  *flags |= FLAG_COMPARE;
                        return Op_feq;
        case Token_Geq: *flags |= FLAG_NOT;
                        [[fallthrough]];
        case Token_Lt:  *flags |= FLAG_COMPARE;
                        return Op_flt;
        case Token_Gt:  *flags |= FLAG_NOT;
                        [[fallthrough]];
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
compiler_binary(Compiler *c, Token const &op, Expr *restrict lhs, Expr *restrict rhs)
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
        lhs->pc    = compiler_code_ABC(c, opcode, INVALID_REG, r1, r2);
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
            case Value_bool: value_set_bool(&rhs->literal, expr_bool(lhs)); break;
            case Value_int:  value_set_int (&rhs->literal, expr_int (lhs)); break;
            case Value_real: value_set_real(&rhs->literal, expr_real(lhs)); break;
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

