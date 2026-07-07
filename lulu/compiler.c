#include "compiler.h"
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
    LULU_LOGF("code[%i] = %u", pc, i);
    chunk_add_instruction(c->L, c->chunk, i);
    return pc;
}

static i32
compiler_code_ABC(Compiler *c, OpCode Op, u8 A, u16 B, u16 C)
{
    return compiler_code(c, instruction_make_ABC(Op, A, B, C));
}

static i32
compiler_code_ABx(Compiler *c, OpCode Op, u8 A, u32 Bx)
{
    return compiler_code(c, instruction_make_ABx(Op, A, Bx));
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
        LULU_ASSERT(e->type && e->type->kind == TypeKind_Atom);
        switch (e->type->atom.kind) {
        case Type_Atom_bool:
            compiler_code_ABx(c, Op_Load_imm, reg, cast(u32)e->literal_bool);
            break;
        case Type_Atom_uint:
            if (e->literal_uint <= INSTRUCTION_Bx_MAX) {
                u32 imm = cast(u32)e->literal_uint;
                compiler_code_ABx(c, Op_Load_imm, reg, imm);
            } else {
                compiler_error(c, "uint literal too large", e);
            }
            break;
        case Type_Atom_int:
            // Use offset-K method for representing negatives.
            if (INSTRUCTION_sBx_MIN <= e->literal_int && e->literal_int <= INSTRUCTION_sBx_MAX) {
                u32 imm = cast(u32)(e->literal_int + INSTRUCTION_sBx_MAX);
                compiler_code_ABx(c, Op_Load_imm, reg, imm);
            } else {
                compiler_error(c, "int literal out of range", e);
            }
            break;
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
        instruction_set_A(&c->chunk->code[e->pc], reg);
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
            e->literal_int = cast(i64)e->literal_uint;
            // Fallthrough
        case Type_Atom_int:  e->literal_int  = -e->literal_int;  return;
        case Type_Atom_real: e->literal_real = -e->literal_real; return;
        default:
            break;
        }
        compiler_expr_any_reg(c, e);
        // TODO(2026-07-07): Determine type in register, emit bytecode
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

// I'm not even going to pretend any of this is remotely sane
#define arg(e, T)      (e)->literal_##T
#define arith(T, op)   arg(lhs, T) op arg(rhs, T)
#define compare(T, op) expr_set_bool(L, lhs, arg(lhs, T) op arg(rhs, T))
#define binary(T)                                                              \
    switch ((op)->kind) {                                                      \
    case Token_Add: arith  (T, +=); break;                                     \
    case Token_Sub: arith  (T, -=); break;                                     \
    case Token_Mul: arith  (T, *=); break;                                     \
    case Token_Div: arith  (T, /=); break;                                     \
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
    lulu_State *L  = c->L;
    if (lhs->kind != rhs->kind || lhs->kind != Expr_Literal) {
        return false;
    }

    LULU_ASSERT(lhs->type->kind == TypeKind_Atom);
    switch (lhs->type->atom.kind) {
    case Type_Atom_uint: binary(uint); break;
    case Type_Atom_int:  binary(int);  break;
    case Type_Atom_real: binary(real); break;
    default:
        return false;
    }
    return true;
}

static void
expr_binary(Expr *lhs, Expr *rhs, i32 pc)
{
    lhs->kind  = Expr_Pending;
    lhs->token = rhs->token;
    lhs->pc    = pc;
}

static OpCode
compiler_binary_opcode(const Token *op, Expr *e)
{
    LULU_ASSERT(e->type->kind == TypeKind_Atom);
    switch (e->type->atom.kind) {
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
        case Token_Mod: return Op_Mod_real;
        default:
            break;
        }
    default:
        break;
    }
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
    u8 o2 = compiler_expr_any_reg(c, rhs);
    LULU_LOGF("rhs = {ExprKind = %i, reg = %u} with o2 = %u and c.free_reg = %u",
        rhs->kind, rhs->reg, o2, c->free_reg);

    u8 o1 = compiler_expr_any_reg(c, lhs);
    LULU_LOGF("lhs = {ExprKind = %i, reg = %u} with o1 = %u and c.free_reg = %u",
        lhs->kind, lhs->reg, o1, c->free_reg);

    if (o1 > o2) {
        expr_pop(c, lhs);
        expr_pop(c, rhs);
    } else {
        expr_pop(c, rhs);
        expr_pop(c, lhs);
    }

    LULU_ASSERT(lhs->type->kind == TypeKind_Atom);
    if (lhs->type == rhs->type) {
        OpCode Op = compiler_binary_opcode(op, lhs);
        expr_binary(lhs, rhs, compiler_code_ABC(c, Op, 0, o1, o2));
    } else {
        compiler_error(c, "Mismatched binary operand",
            expr_is_numeric(lhs) ? rhs : lhs);
    }
}

#undef binary
#undef compare
#undef arith
#undef arg

LULU_INTERNAL_FUNC void
compiler_return(Compiler *c, Expr *e)
{
    u8 reg = compiler_expr_any_reg(c, e);
    compiler_code_ABC(c, Op_Return, reg, 1, 1);
}
