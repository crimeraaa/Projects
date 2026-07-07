#include <stdio.h>

#include "debug.h"

LULU_INTERNAL_FUNC void
debug_disassemble(const Chunk *c)
{
    printf("=== DISASSEMBLY ===\n");
    printf(".code:\n");
    for (usize i = 0; i < c->code_len; i++) {
        debug_disassemble_at(c, i);
    }
    printf("===================\n");
}

static const char *
debug_opcode_name(OpCode Op)
{
    switch (Op) {
    case Op_None:      break;
    case Op_Load_imm:  return "load.imm";
    case Op_Load_int:  return "load.int";
    case Op_Load_real: return "load.f64";
    case Op_Neg_int:   return "neg.int";
    case Op_Add_int:   return "add.int";
    case Op_Sub_int:   return "sub.int";
    case Op_Mul_int:   return "mul.int";
    case Op_Div_uint:  return "div.uint";
    case Op_Mod_uint:  return "mov.uint";
    case Op_Div_int:   return "div.int";
    case Op_Mod_int:   return "mod.int";
    case Op_Neg_real:  return "neg.f64";
    case Op_Add_real:  return "add.f64";
    case Op_Sub_real:  return "sub.f64";
    case Op_Mul_real:  return "mul.f64";
    case Op_Div_real:  return "div.f64";
    case Op_Mod_real:  return "mod.f64";
    case Op_Return:    return "return";
    }
    LULU_UNREACHABLE();
    return nullptr;
}

LULU_INTERNAL_FUNC void
debug_disassemble_at(const Chunk *c, usize offset)
{
    Instruction i  = c->code[offset];
    OpCode      Op = instruction_get_Op(i);
    u8          A  = instruction_get_A(i);
    u16         B  = instruction_get_B(i);
    u16         C  = instruction_get_C(i);

    printf("| %-9s %-4u %-9u %-9u\n", debug_opcode_name(Op), A, B, C);
}
