#include <limits.h> // CHAR_BIT
#include <stdio.h>

#include "debug.h"

static void
print_bool(bool b)
{
    printf("bool = %s", b ? "true" : "false");
}

static void
print_uint(lulu_uint value, lulu_uint base, bool print_type)
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
            lulu_uint digit = value % base;
            *p-- = DIGITS[digit];
            value   /= base;
        }
        fputs(p, stdout);
    }
}

static void
print_int(lulu_int i)
{
    lulu_uint u = cast(lulu_uint)i;
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
    case Value_nil:  fputs("nil", stdout);            break;
    case Value_bool: print_bool(v.value.b);           break;
    case Value_uint: print_uint(v.value.u, 10, true); break;
    case Value_int:  print_int (v.value.i);           break;
    case Value_real: print_real(v.value.f);           break;
    default:
        LULU_PANICF("Unprintable ValueKind(%i)", v.kind);
        break;
    }
}

LULU_INTERNAL_FUNC void
debug_disassemble(Chunk const *c)
{
    printf("======== DISASSEMBLY ========\n");

    if (c->values_len > 0) {
        printf(".values:\n");
        for (usize i = 0; i < c->values_len; i++) {
            printf("| [%zu]: ", i);
            print_tvalue(c->values[i]);
            putc('\n', stdout);
        }
    }

    printf(".code:\n");
    for (usize i = 0; i < c->code_len; i++) {
        debug_disassemble_at(c, i);
    }
    printf("=============================\n");
}

static char const *OPCODE_NAMES[] = {
#define X(e, s, fmt) s,
    OPCODE_KINDS(X)
#undef X
};

LULU_INTERNAL_FUNC void
debug_disassemble_at(Chunk const *c, usize offset)
{
    Instruction   i      = c->code[offset];
    OpCode const  opcode = GET_OPCODE(i);
    char   const *opname = OPCODE_NAMES[opcode];
    u8     const  A      = GETARG_A(i);

    printf("| %-16s %-3u ", opname, A);
    switch (opform_get(opcode)) {
    case OpForm_ABC:
        printf("%-3u %-5u", GETARG_B(i), GETARG_C(i));
        break;
    case OpForm_vABC:
        printf("%-3u %-3u %u", GETARG_B(i), GETARG_vC(i), GETARG_k(i));
        break;
    case OpForm_ABx:
        printf("%-9u", GETARG_Bx(i));
        break;
    case OpForm_AsBx:
        printf("%-9i", GETARG_sBx(i));
        break;
    }
    printf("\n");
}
