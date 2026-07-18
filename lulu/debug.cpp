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
            u64 digit = value % base;
            *p-- = DIGITS[digit];
            value   /= base;
        }
        fputs(p, stdout);
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
        putc('-', stdout);
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
    printf("======== DISASSEMBLY ========\n");

    if (c->values_cap > 0) {
        printf(".values:\n");
        for (usize i = 0; i < c->values_cap; i++) {
            printf("| [%zu]: ", i);
            print_tvalue(c->values[i]);
            putc('\n', stdout);
        }
    }

    printf(".code:\n");
    for (usize i = 0; i < c->code_cap; i++) {
        debug_disassemble_at(c, i);
    }
    printf("=============================\n");
}

static char const *OPCODE_CSTRINGS[] = {
#define X(e, ...) #e,
    OPCODE_KINDS(X)
#undef X
};

LULU_INTERNAL_FUNC void
debug_disassemble_at(Chunk const *c, usize offset)
{
    Instruction i  = c->code[offset];
    OpCode      op = GET_OPCODE(i);
    u8          A  = GETARG_A(i);

    printf("| %-8s %-3u ", OPCODE_CSTRINGS[op], A);
    switch (OPCODE_INFO_FORMAT(op)) {
    case OpForm_ABC:
        printf("%-3u %-7u", GETARG_B(i), GETARG_C(i));
        break;
    case OpForm_ABx:
        printf("%-11u", GETARG_Bx(i));
        break;
    case OpForm_AsBx:
        printf("%-11i", GETARG_sBx(i));
        break;
    case OpForm_vABC:
        printf("%-3u %-3u k=%u", GETARG_B(i), GETARG_vC(i), GETARG_k(i));
        break;
    case OpForm_vABx:
        printf("%-7u k=%u", GETARG_vBx(i), GETARG_k(i));
        break;
    case OpForm_vAsBx:
        printf("%-7i k=%u", GETARG_vsBx(i), GETARG_k(i));
        break;
    }

    printf("\n");
}
