#include <limits.h> // CHAR_BIT
#include <stdio.h>

#include "debug.hpp"
#include "opcode.hpp"
#include "chunk.hpp"


static void
print_bool(bool b)
{
    printf("bool = %s", b ? "true" : "false");
}

static void
print_uint(u64 value, u64 base, bool print_type)
{
    // base-2 up to and including base-36.
    static char const
    DIGITS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    if (print_type) {
        fputs("uint = ", stdout);
    }

    if (value == 0) {
        fputc(DIGITS[value], stdout);
    } else {
        // Should be enough for the largest unsigned binary string.
        char  buf[sizeof(value) * CHAR_BIT + 1];
        char *p = buf + (sizeof(buf) - 1);

        // Ensure nul-termination.
        *p-- = 0;

        // Write digits, from LSD to MSD, in reverse order.
        while (value > 0) {
            u64 d  = value % base;
            *p--   = DIGITS[d];
            value /= base;
        }

        // We always point to the first unwritten buffer slot.
        fputs(p + 1, stdout);
    }
}

static void
print_int(lulu_int i)
{
    auto u = cast(u64)i;
    fputs("int = ", stdout);
    if (i < 0) {
        // Well-defined absolute value which works for min(T).
        // Don't negate directly to avoid warnings with MSVC.
        u = 0 - u;
        fputc('-', stdout);
    }
    print_uint(u, 10, false);
}

static void
print_real(lulu_real r)
{
    printf("real = %.14g", r);
}

static void
print_tvalue(TValue v)
{
    switch (v.kind) {
    case Value_nil:  fputs("nil", stdout);       break;
    case Value_bool: print_bool(tvalue_bool(v)); break;
    case Value_int:  print_int (tvalue_int(v));  break;
    case Value_real: print_real(tvalue_real(v)); break;
    default:
        LULU_PANICF("Unprintable ValueKind(%i)", v.kind);
        break;
    }
}

LULU_INTERNAL_FUNC void
debug_disassemble(Chunk const *c)
{
    usize n = 0;

    printf("======== DISASSEMBLY ========\n");
    if (len(c->constants) > 0) {
        printf(".values:\n");
        for (TValue k : c->constants) {
            printf("| ");
            print_tvalue(k);
            putc('\n', stdout);
        }
    }

    if (len(c->reg_info) > 0) {
        printf(".stack:\n");
        for (RegInfo r : c->reg_info) {
            printf("| pc[%i, %i] ; reg = %i, type = %p\n",
                r.pc_born, r.pc_died, r.reg, cast(void *)r.type);
        }
    }

    printf(".code:\n");
    for (usize i = 0, n = len(c->code); i < n; i++) {
        debug_disassemble_at(c, i);
    }
    printf("=============================\n");
}

static char const *const OPCODE_CSTRINGS[] = {
#define X(e, ...) #e,
    OPCODE_KINDS(X)
#undef X
};

LULU_INTERNAL_FUNC void
debug_disassemble_at(Chunk const *c, usize offset)
{
    Instruction i  = c->code[offset];
    OpCode      op = get_opcode(i);
    u8          A  = getarg_A(i);

    printf("| %-8s %-3u ", OPCODE_CSTRINGS[op], A);
    switch (OPCODE_INFO_FORMAT(op)) {
    case OpForm_ABC:
        printf("%-3u %-7u", getarg_B(i), getarg_C(i));
        break;
    case OpForm_ABx:
        printf("%-11u", getarg_Bx(i));
        break;
    case OpForm_AsBx:
        printf("%-11i", getarg_sBx(i));
        break;
    case OpForm_vABC:
        printf("%-3u %-3u k=%u", getarg_B(i), getarg_vC(i), getarg_k(i));
        break;
    case OpForm_vABx:
        printf("%-7u k=%u", getarg_vBx(i), getarg_k(i));
        break;
    case OpForm_vAsBx:
        printf("%-7i k=%u", getarg_vsBx(i), getarg_k(i));
        break;
    }

    printf("\n");
}
