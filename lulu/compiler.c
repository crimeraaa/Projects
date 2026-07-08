#include <math.h> // trunc

#include "compiler.h"
#include "expr.h"
#include "internal.h"
#include "opcode.h"
#include "type.h"

static void
compiler_error(Compiler *c, const char *info, Expr *e)
{
    parser_error_at(c->parser, info, &e->token);
}

LULU_INTERNAL_FUNC void
compiler_cast(Compiler *c, const Type *t, Expr *e)
{
    compiler_error(c, "`cast` not yet implemented", e);
}

static void
reg_pop(Compiler *c, u8 reg)
{
    if (reg >= c->active_count) {
        c->free_reg--;
        LULU_ASSERTF(reg == c->free_reg,
            "Expected free_reg = %u but got %u",
            reg, c->free_reg);
    }
}

static void
reg_push(Compiler *c, u8 reg_count)
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

static i32
compiler_code(Compiler *c, Instruction i)
{
    i32 pc = cast(i32)c->chunk->code_len;
    chunk_add_instruction(c->L, c->chunk, i);
    return pc;
}

static i32
compiler_code_ABC(Compiler *c, OpCode Op, u8 A, u16 B, u16 C)
{
    return compiler_code(c, MAKE_ABC(Op, A, B, C));
}

static i32
compiler_code_ABx(Compiler *c, OpCode Op, u8 A, u32 Bx)
{
    return compiler_code(c, MAKE_ABx(Op, A, Bx));
}

static void
expr_discharge_vars(Compiler *c, Expr *e)
{
    unused(c);
    unused(e);
}

static void
expr_discharge_reg(Compiler *c, Expr *e, u8 reg)
{
    expr_discharge_vars(c, e);
    switch (e->kind) {
    case Expr_Literal: 
        switch (expr_get_type_atom(e)->kind) {
        case Type_Atom_bool:
            compiler_code_ABx(c, Op_Load_imm, reg, cast(u32)e->literal_bool);
            break;
        case Type_Atom_uint: {
            lulu_uint imm = expr_get_uint(e);
            if (imm <= ARG_Bx_MAX) {
                compiler_code_ABx(c, Op_Load_imm, reg, cast(u32)imm);
            } else {
                compiler_error(c, "uint literal too large", e);
            }
            break;
        }

        case Type_Atom_int: {
            lulu_int imm = expr_get_int(e);
            // Use offset-K method for representing negatives.
            if (ARG_sBx_MIN <= imm && imm <= ARG_sBx_MAX) {
                imm += ARG_sBx_MAX;
                compiler_code_ABx(c, Op_Load_imm, reg, cast(u32)imm);
            } else {
                compiler_error(c, "int literal out of range", e);
            }
            break;
        }

        case Type_Atom_real:
            compiler_error(c, "real literals not yet implemented", e);
            break;
        default:
            compiler_error(c, "Unsupported literal", e);
            break;
        }
        break;
    case Expr_Discharged:
        if (reg != e->reg) {
            compiler_error(c, "Op_Move not yet implemented", e);
        }
        break;
    case Expr_Pending:
        SETARG_A(&c->chunk->code[e->pc], reg);
        break;
    default:
        LULU_ASSERT(!e->kind);
        break;
    }

    e->kind = Expr_Discharged;
    e->reg  = reg;
}

static u8
expr_to_reg(Compiler *c, Expr *e, u8 reg)
{
    expr_discharge_reg(c, e, reg);
    e->kind = Expr_Discharged;
    e->reg  = reg;
    return reg;
}


LULU_INTERNAL_FUNC u8
compiler_expr_next_reg(Compiler *c, Expr *e)
{
    expr_discharge_vars(c, e);
    expr_pop(c, e);
    reg_push(c, 1);
    return expr_to_reg(c, e, c->free_reg - 1);
}

LULU_INTERNAL_FUNC u8
compiler_expr_any_reg(Compiler *c, Expr *e)
{
    expr_discharge_vars(c, e);
    if (e->kind == Expr_Discharged) {
        if (e->reg >= c->active_count) {
            return expr_to_reg(c, e, e->reg);
        }
    }
    return compiler_expr_next_reg(c, e);
}

LULU_INTERNAL_FUNC void
compiler_unary(Compiler *c, const Token *op, Expr *e)
{
    switch (op->kind) {
    case Token_Sub:
        if (e->kind == Expr_Literal) switch (e->type->atom.kind) {
        case Type_Atom_uint:
            // Since it's untyped, coerce it to signed.
            e->type        = type_atom_get(c->L, Type_Atom_int);
            e->literal_int = cast(lulu_int)e->literal_uint;
            // Fallthrough
        case Type_Atom_int:  e->literal_int  = -e->literal_int;  return;
        case Type_Atom_real: e->literal_real = -e->literal_real; return;
        default:
            break;
        }
        compiler_expr_any_reg(c, e);
        break;
    default:
        break;
    }
    parser_error_at(c->parser, "Unsupported unary operand", op);
}

static void
expr_set_bool(lulu_State *L, Expr *e, bool b)
{
    e->kind         = Expr_Literal;
    e->type         = type_atom_get(L, Type_Atom_bool);
    e->literal_bool = b;
}

static bool
compiler_coerce_signs(Compiler *c, Expr *u, Expr *i)
{
    // All positive signed integers can be coerced to unsigned integers.
    lulu_int i_imm = expr_get_int(i);
    if (i_imm >= 0) {
        i->literal_uint = cast(lulu_uint)i_imm;
        i->type         = u->type;
    }
    // Otherwise, try to coerce the unsigned integer to a signed one.
    else {
        lulu_uint u_imm = expr_get_uint(u);
        // Can't possibly be represented as a signed integer.
        if (u_imm > INT64_MAX) {
            return false;
        }
        u->literal_int = cast(lulu_int)u_imm;
        u->type        = i->type;
    }
    return true;
}

static bool
compiler_coerce_uint_real(Compiler *c, Expr *u, Expr *r)
{
    lulu_uint u_imm = expr_get_uint(u);
    u->literal_real = cast(lulu_real)u_imm;
    u->type         = r->type;
    return true;
}

static bool
compiler_coerce_int_real(Compiler *c, Expr *i, Expr *r)
{
    lulu_int  i_imm = expr_get_int(i);
    i->literal_real = cast(lulu_real)i_imm;
    i->type         = r->type;
    return true;
}


static bool
compiler_coerce_numeric(Compiler *c, Expr *lhs, Expr *rhs)
{
    if (expr2_both_literal(lhs, rhs)) switch (expr_get_type_atom(lhs)->kind) {
    case Type_Atom_uint:
        switch (expr_get_type_atom(rhs)->kind) {
        case Type_Atom_uint: LULU_UNREACHABLE(); break;
        case Type_Atom_int:  return compiler_coerce_signs(c, lhs, rhs);
        case Type_Atom_real: return compiler_coerce_uint_real(c, lhs, rhs);
        default:
            break;
        }
        break;
    case Type_Atom_int:
        switch (expr_get_type_atom(rhs)->kind) {
        case Type_Atom_uint: return compiler_coerce_signs(c, rhs, lhs);
        case Type_Atom_int:  LULU_UNREACHABLE(); break;
        case Type_Atom_real: return compiler_coerce_int_real(c, lhs, rhs);
        default:
            break;
        }
        break;
    case Type_Atom_real:
        // TODO(2026-07-08): Do we really want propagation?
        switch (expr_get_type_atom(rhs)->kind) {
        case Type_Atom_uint: return compiler_coerce_uint_real(c, rhs, lhs);
        case Type_Atom_int:  return compiler_coerce_int_real(c, rhs, lhs);
        case Type_Atom_real: LULU_UNREACHABLE(); break;
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
#define div(T, op)     if (arg(rhs, T) == 0) return false; arith(T, op)
#define mod            div
#define compare(T, op) expr_set_bool(L, lhs, arg(lhs, T) op arg(rhs, T))
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
compiler_folded_arith(Compiler *c, const Token *op, Expr *lhs, Expr *rhs)
{
    if (lhs->kind != rhs->kind) {
        if (!compiler_coerce_numeric(c, lhs, rhs)) {
            return false;
        }
    }

    lulu_State *L = c->L;
    switch (expr_get_type_atom(lhs)->kind) {
    case Type_Atom_uint: binary(uint); break;
    case Type_Atom_int:  binary(int);  break;
#undef mod
#define mod(...) return false
    case Type_Atom_real: binary(real); break;
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

static void
expr_binary(Expr *lhs, Expr *rhs, i32 pc)
{
    lhs->kind  = Expr_Pending;
    lhs->token = rhs->token;
    lhs->pc    = pc;
}

static OpCode
compiler_binary_op(Compiler *c, const Token *op, Expr *e)
{
    switch (expr_get_type_atom(e)->kind) {
    case Type_Atom_uint:
        switch (op->kind) {
        case Token_Add: return Op_Add_int;
        case Token_Sub: return Op_Sub_int;
        case Token_Mul: return Op_Mul_int;
        case Token_Div: return Op_Div_uint;
        case Token_Mod: return Op_Mod_uint;
        default:
            break;
        }
    case Type_Atom_int:
        switch (op->kind) {
        case Token_Add: return Op_Add_int;
        case Token_Sub: return Op_Sub_int;
        case Token_Mul: return Op_Mul_int;
        case Token_Div: return Op_Div_int;
        case Token_Mod: return Op_Mod_int;
        default:
            break;
        }
    case Type_Atom_real:
        switch (op->kind) {
        case Token_Add: return Op_Add_real;
        case Token_Sub: return Op_Sub_real;
        case Token_Mul: return Op_Mul_real;
        case Token_Div: return Op_Div_real;
        case Token_Mod:
            compiler_error(c, "Modulo not supported for reals- consider using `fmod()`", e);
            break;
        default:
            break;
        }
    default:
        break;
    }
    LULU_PANICF("Invalid Token_Kind(%i) and/or ExprKind(%i)", op->kind, e->kind);
    LULU_UNREACHABLE();
    return Op_None;
}

LULU_INTERNAL_FUNC void
compiler_binary(Compiler *c, const Token *op, Expr *lhs, Expr *rhs)
{
#if PARSER_CONSTANT_FOLDING
    if (compiler_folded_arith(c, op, lhs, rhs)) {
        return;
    }
#endif

    u8 r1 = expr_get_reg(lhs);
    u8 r2 = compiler_expr_any_reg(c, rhs);
    if (r1 > r2) {
        expr_pop(c, lhs);
        expr_pop(c, rhs);
    } else {
        expr_pop(c, rhs);
        expr_pop(c, lhs);
    }

    OpCode Op = compiler_binary_op(c, op, lhs);
    expr_binary(lhs, rhs, compiler_code_ABC(c, Op, 0, r1, r2));
}

LULU_INTERNAL_FUNC void
compiler_return(Compiler *c, Expr *e)
{
    u8 reg = compiler_expr_any_reg(c, e);
    compiler_code_ABC(c, Op_Return, reg, 1, 1);
}
