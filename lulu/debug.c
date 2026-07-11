#include <limits.h> // CHAR_BIT
#include <stdio.h>

#include "debug.h"

static void
print_bool(bool b)
{
    printf("bool = %s", b ? "true" : "false");
}

static void
print_uint(lulu_uint u, bool print_type)
{
    static const char DIGITS[] = "0123456789";
    if (print_type) {
        fputs("uint = ", stdout);
    }

    if (u == 0) {
        fputc(DIGITS[u], stdout);
    } else {
        lulu_uint base = 10;
        char      buf[sizeof(u) * CHAR_BIT];
        char *    p = buf + (sizeof(buf) - 1);

        // Ensure nul-termination.
        *p-- = 0;

        // Write digits, from LSD to MSD, in reverse order.
        while (u > 0) {
            lulu_uint d = u % base;
            *p-- = DIGITS[d];
            u   /= base;
        }
        fputs(p, stdout);
    }
}

static void
print_int(lulu_int i)
{
    fputs("int = ", stdout);
    if (i >= 0) {
        print_uint(cast(lulu_uint)i, /*print_type=*/false);
    } else {
        // Well-defined absolute value which works for min(T).
        lulu_uint u = 0 - cast(lulu_uint)i;
        putc('-', stdout);
        print_uint(u, /*print_type=*/false);
    }
}

static void
print_real(lulu_real r)
{
    printf("real = %.14g", r);
}

LULU_INTERNAL_FUNC void
debug_disassemble(const Chunk *c)
{
    printf("======== DISASSEMBLY ========\n");

    if (c->values_len > 0) {
        printf(".values:\n");
        for (usize i = 0; i < c->values_len; i++) {
            TValue v = c->values[i];
            printf("| [%zu]: ", i);
            switch (v.kind) {
            case Value_none:    LULU_UNREACHABLE();          break;
            case Value_nil:     fputs("nil", stdout);        break;
            case Value_bool:    print_bool(v.value.b);       break;
            case Value_uint:    print_uint(v.value.u, true); break;
            case Value_int:     print_int (v.value.i);       break;
            case Value_real:    print_real(v.value.f);       break;
            case Value_string:
                LULU_PANIC();
                break;
            }
            putc('\n', stdout);
        }
    }

    printf(".code:\n");
    for (usize i = 0; i < c->code_len; i++) {
        debug_disassemble_at(c, i);
    }
    printf("=============================\n");
}

typedef struct OpInfo OpInfo;
struct OpInfo {
    OpFormat    format;
    const char *name;
};

static const OpInfo OPCODE_INFO[] = {
#define X(e, s, fmt) {f##fmt, s},
    OPCODE_KINDS(X)
#undef X
};

LULU_INTERNAL_FUNC void
debug_disassemble_at(const Chunk *c, usize offset)
{
    Instruction  i    = c->code[offset];
    const OpCode Op   = GET_OPCODE(i);
    const OpInfo info = OPCODE_INFO[Op];
    const u8     A    = GETARG_A(i);

    printf("| %-9s %-3u ", info.name, A);
    switch (info.format) {
    case fABC:  printf("%-3u %-3u", GETARG_B(i), GETARG_C(i)); break;
    case fABx:  printf("%-7u",      GETARG_Bx(i));             break;
    case fAsBx: printf("%-7i",      GETARG_sBx(i));            break;
    }
    printf("\n");
}
