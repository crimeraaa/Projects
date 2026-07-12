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

#define compiler_code_ABC(c, Op, A, B, C)   compiler_code(c, MAKE_ABC(Op, A, B, C))
#define compiler_code_ABx(c, Op, A, Bx)     compiler_code(c, MAKE_ABx(Op, A, Bx));

static void
expr_set_bool_literal(Expr *e, lulu_bool b)
{
    e->kind         = Expr_Literal;
    e->type         = atom_type_get(Value_bool);
    e->literal_bool = b;
}

static void
expr_set_uint_literal(Expr *e, lulu_uint u)
{
    e->kind         = Expr_Literal;
    e->type         = atom_type_get(Value_uint);
    e->literal_uint = u;
}

static void
expr_set_int_literal(Expr *e, lulu_int i)
{
    e->kind        = Expr_Literal;
    e->type        = atom_type_get(Value_int);
    e->literal_int = i;
}

static void
expr_set_real_literal(Expr *e, lulu_real r)
{
    e->kind         = Expr_Literal;
    e->type         = atom_type_get(Value_real);
    e->literal_real = r;
}

// I'm not even going to pretend this is remotely sane
// Marginal waste of time when the type matches its literals, but that's
// not common behavior anyway.
#define compiler_cast_type(fn, T)                                              \
    ValueKind vk = expr_atom_kind(e);                                          \
    switch (e->kind) {                                                         \
    case Expr_Literal:                                                         \
        switch (vk) {                                                          \
        case Value_bool: fn(e, cast(lulu_##T)e->literal_bool); return;         \
        case Value_uint: fn(e, cast(lulu_##T)e->literal_uint); return;         \
        case Value_int:  fn(e, cast(lulu_##T)e->literal_int);  return;         \
        case Value_real: fn(e, cast(lulu_##T)e->literal_real); return;         \
        default:                                                               \
            break;                                                             \
        }                                                                      \
        break;                                                                 \
    case Expr_Pending:                                                         \
        compiler_expr_any_reg(c, e);                                           \
        /* fallthrough */                                                      \
    case Expr_Discharged: {                                                    \
        ValueKind tag = Value_##T;                                             \
        compiler_code_ABC(c, Op_cast, e->reg, cast(u16)tag, cast(u16)vk);      \
        e->type = atom_type_get(tag);                                          \
        return;                                                                \
    }                                                                          \
    default:                                                                   \
        break;                                                                 \
    }                                                                          \
    compiler_error(c, "Cannot cast to '" #T "'", e);                           \

static void
compiler_cast_bool(Compiler *c, Expr *e)
{
    compiler_cast_type(expr_set_bool_literal, bool);
}

static void
compiler_cast_uint(Compiler *c, Expr *e)
{
    compiler_cast_type(expr_set_uint_literal, uint);
}

static void
compiler_cast_int(Compiler *c, Expr *e)
{
    compiler_cast_type(expr_set_int_literal, int);
}

static void
compiler_cast_real(Compiler *c, Expr *e)
{
    compiler_cast_type(expr_set_real_literal, real);
}

#undef compiler_cast_type

LULU_INTERNAL_FUNC void
compiler_cast(Compiler *c, Type const *t, Expr *e)
{   
    // Nothing to do?
    if (e->type == t) {
        return;
    }

    switch (t->kind) {
    case TypeKind_Atom:
        switch (t->atom.kind) {
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
        LULU_ASSERTF(reg == c->free_reg,
            "Expected free_reg = %u but got %u",
            reg, c->free_reg);
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
    unused(c);
    unused(e);
}

static void
compiler_load_bool(Compiler *c, u16 reg, bool b)
{
    compiler_code_ABC(c, Op_bool, reg, cast(u16)b, 0);
}

static void
compiler_load_uint(Compiler *c, u16 reg, lulu_uint u)
{
    if (u <= ARG_Bx_MAX) {
        compiler_code_ABx(c, Op_uint_imm, reg, cast(u32)u);
    } else {
        TValue tv;
        u32    k;
        tv.kind    = Value_uint;
        tv.value.u = u;
        k          = chunk_add_constant(c->L, c->chunk, tv);
        compiler_code_ABx(c, Op_uint_k, reg, k);
    }
}

static void
compiler_load_int(Compiler *c, u16 reg, lulu_int i)
{
    if (ARG_sBx_MIN <= i && i <= ARG_sBx_MAX) {
        compiler_code_ABx(c, Op_int_imm, reg, cast(u32)(i + ARG_sBx_MAX));
    } else {
        TValue tv;
        u32    k;
        tv.kind    = Value_int;
        tv.value.i = i;
        k          = chunk_add_constant(c->L, c->chunk, tv);
        compiler_code_ABx(c, Op_int_k, reg, k);
    }
}

static void
compiler_load_real(Compiler *c, u16 reg, lulu_real r)
{
    TValue tv;
    u32    k;
    tv.kind    = Value_real;
    tv.value.f = r;
    k = chunk_add_constant(c->L, c->chunk, tv);
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
        switch (expr_atom_kind(e)) {
        case Value_bool: compiler_load_bool(c, reg, e->literal_bool); break;
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
            compiler_error(c, "Op_move not yet implemented", e);
        }
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
        // Register does not refer to an active local variable slot?
        if (e->reg >= c->active_count) {
            return expr_to_reg(c, e, e->reg);
        }
    }
    return compiler_expr_next_reg(c, e);
}

static OpCode
compiler_unary_op(Token const *op, Expr *e)
{
    switch (op->kind) {
    case Token_Len:
        if (type_is_atom(e->type)) switch (e->type->atom.kind) {
        case Value_uint:
        case Value_int:  return Op_neg;
        case Value_real: return Op_fneg;
        default:        break;
        }
        break;
    case Token_Sub:
        // Unary negation only applies to arithmetic types.
        if (type_is_atom(e->type)) switch (e->type->atom.kind) {
        case Value_uint:
        case Value_int:  return Op_sub;
        case Value_real: return Op_fsub;
        default:        break;
        }
        break;
    case Token_not:
        if (expr_has_atom_kind(e, Value_bool)) {
            return Op_not;
        }
    default:
        break;
    }
    return Op_None;
}

static bool
compiler_unary_folded(Compiler *c, Token const *op, Expr *e)
{
    if (expr_is_literal(e)) switch (op->kind) {
    case Token_Sub:
        switch (e->type->atom.kind) {
        case Value_uint:
            // TODO(2026-07-11): Do we want to check for overflow?
            e->type        = atom_type_get(Value_int);
            e->literal_int = -cast(lulu_int)e->literal_uint;
            return true;
        case Value_int:  e->literal_int  = -e->literal_int;  return true;
        case Value_real: e->literal_real = -e->literal_real; return true;
        default:
            break;
        }
        break;
    case Token_not:
        if (expr_has_atom_kind(e, Value_bool)) {
            e->literal_bool = !e->literal_bool;
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

LULU_INTERNAL_FUNC void
compiler_unary(Compiler *c, Token const *op, Expr *e)
{
#if PARSER_CONSTANT_FOLDING
    if (compiler_unary_folded(c, op, e)) {
        return;
    }
#endif
    OpCode opcode = compiler_unary_op(op, e);
    if (!opcode) {
        compiler_error(c, "Invalid unary operand", e);
        return;
    }

    u16 r1  = compiler_expr_any_reg(c, e);
    e->kind = Expr_Pending;
    e->pc   = compiler_code_ABC(c, opcode, INVALID_REG, cast(u16)r1, 0);
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

static bool
compiler_implicit_cast_numeric(Compiler *c, Expr *lhs, Expr *rhs)
{
    ValueKind lhs_kind, rhs_kind;

    lhs_kind = expr_atom_kind(lhs);
    rhs_kind = expr_atom_kind(rhs);
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
compiler_binary_folded(Compiler *c, Token const *op, Expr *lhs, Expr *rhs)
{
    if (!expr2_both_literal(lhs, rhs)) {
        return false;
    } else if (lhs->type != rhs->type) {
        if (!compiler_implicit_cast_numeric(c, lhs, rhs)) {
            return false;
        }
    }

#if !PARSER_CONSTANT_FOLDING
    return false;
#endif

    switch (expr_atom_kind(lhs)) {
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

#define FLAG_SWITCH     (1 << 0)
#define FLAG_NOT        (1 << 1)

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

    if (type_is_atom(lhs->type)) switch (expr_atom_kind(lhs)) {
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
        case Token_Eq:  return Op_eq;
        case Token_Gt:  *flags |= FLAG_SWITCH;
        case Token_Lt:  return Op_lt;
        case Token_Geq: *flags |= FLAG_SWITCH;
        case Token_Leq: return Op_leq;
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
        case Token_Eq:  return Op_eq;
        case Token_Gt:  *flags |= FLAG_SWITCH;
        case Token_Lt:  return Op_ilt;
        case Token_Geq: *flags |= FLAG_SWITCH;
        case Token_Leq: return Op_ileq;
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
        case Token_Eq:  return Op_feq;
        case Token_Gt:  *flags |= FLAG_SWITCH;
        case Token_Lt:  return Op_flt;
        case Token_Geq: *flags |= FLAG_SWITCH;
        case Token_Leq: return Op_fleq;
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
        u16 tmp = r1;
        r1 = r2;
        r2 = tmp;
    }

    lhs->kind  = Expr_Pending;
    lhs->token = rhs->token;
    lhs->pc    = compiler_code_ABC(c, opcode, INVALID_REG, r1, r2);
    if (flags & FLAG_NOT) {
        u16 rself = compiler_expr_next_reg(c, lhs);
        expr_pop(c, lhs);

        lhs->kind = Expr_Pending;
        lhs->pc   = compiler_code_ABC(c, Op_not, INVALID_REG, rself, 0);
    }
}

#undef FLAG_NOT
#undef FLAG_SWITCH

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

static bool
compiler_implicit_cast_rhs(Compiler *c, Type const *type, Expr *rhs)
{
    // If rhs isn't a literal and it's of the wrong type, then we don't
    // allow it to assign to lhs as implicit casts are error-prone.
    if (!type_is_atom(type) || !expr_is_literal(rhs)) {
        return false;
    }

    Type const *rtype = rhs->type;
    switch (type->atom.kind) {
    case Value_uint:
        switch (rtype->atom.kind) {
        case Value_int:  return compiler_implicit_uint_from_int (rhs);
        case Value_real: return compiler_implicit_uint_from_real(rhs);
        default:         break;
        }
        break;
    case Value_int:
        switch (rtype->atom.kind) {
        // <ident> : 'int' = <uint-literal>
        case Value_uint: return compiler_implicit_int_from_uint(rhs);
        case Value_real: return compiler_implicit_int_from_real(rhs);
        default:         break;
        }
        break;
    case Value_real:
        switch (rtype->atom.kind) {
        case Value_uint: return compiler_implicit_real_from_uint(rhs);
        case Value_int:  return compiler_implicit_real_from_int (rhs);
        default:         break;
        }
        break;
    default: break;
    }
    return false;
}

LULU_INTERNAL_FUNC void
compiler_declare(Compiler *c, Expr *lhs, Expr *rhs)
{
    // No assigning expression given, so use the zero value.
    if (!rhs->kind) {
        switch (lhs->type->kind) {
        case TypeKind_Atom:
            switch (lhs->type->atom.kind) {
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

    u16 const reg = compiler_expr_next_reg(c, rhs);
    Variable *v   = &c->variables[c->active_count++];
    LULU_ASSERT(reg == c->active_count - 1);

    v->type   = lhs->type;
    v->name   = lhs->token.lexeme;
    v->scope  = 1;
    lhs->reg  = reg;
}

#undef compiler_code_ABx
#undef compiler_code_ABC
