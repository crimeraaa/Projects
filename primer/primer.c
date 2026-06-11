// standard
#include <inttypes.h> // PRI* macros

// local
#include "primer.h"

// Modulo where `y`, the divisor, is a power of 2.
#define mod2(x, y)  ((x) & ((y) - 1))

#if 0
#define dprintf(...)    printf(__VA_ARGS__)

static const char *
binle(LIMB_TYPE value)
{
    static char buf[BIT_SIZE(value) + 1];
    // Print LSB to MSB.
    LIMB_TYPE i, bit;
    for (i = 0; i < sizeof(buf) - 1; i += 1) {
        bit     = (value >> i) & 1;
        buf[i] = (bit) ? '1' : '0';
    }
    return buf;
}

#else
#define dprintf(...)
#define binle(x)            unused(x)
#endif

typedef struct Strand_Index Strand_Index;
struct Strand_Index {
    u64 limb_index, bit_index;
};

static Strand_Index
strand_index_make(u64 base_index)
{
    Strand_Index i = {0, 0};

    // Get 0-based index of the limb which contains `index`.
    // Note that unsigned integer division always returns the floor.
    //
    // I.e.  indexes in the range [0,32)  give us limb index 0,
    // while indexes in the range [32,64) give us limb index 1,
    // and   indexes in the range [64,96) give us limb index 2.
    i.limb_index = (base_index * BASE_BITS) / LIMB_TYPE_BITS,

    // Get the index of the bit within the limb we want to manipulate.
    // Note that since bases are 2-bits, our bit indexes are likewise
    // always multiples of 2.
    //
    // I.e. index == 0 should give us 0,
    // but  index == 1 should give us 2 since the 0th base occupies [0, 1].
    // and  index == 3 should give us 4 since indexes [0, 3] are used.
    i.bit_index = cast(LIMB_TYPE)mod2(base_index * BASE_BITS, LIMB_TYPE_BITS);
    return i;
}

static void
strand_set(Strand *s, Strand_Index i, enum Base base)
{
    LIMB_TYPE *limb_ptr, mask;
    limb_ptr  = &s->limbs.data[i.limb_index];

    // Clear out 2-bit value at this bit index before setting in the new value
    // as given by `base`.
    mask      = ~(BASE_MAX << i.bit_index);
    *limb_ptr = (*limb_ptr & mask) | (cast(LIMB_TYPE)base << i.bit_index);

    dprintf("l=%" PRIu64  ", b=%02" PRIu64 ":%02" PRIu64 "] = %10u // %s\n",
            i.limb_index, i.bit_index, i.bit_index + 2, *limb_ptr, binle(*limb_ptr));
}

static Strand
strand_make(String dna)
{
    Strand s = {NULL, 0, 0};
    for (const char *it = dna.data, *end = it + dna.len; it < end; it += 1) {
        Strand_Index i;
        enum Base b;
        switch (*it) {
        case 'a': case 'A': b = BASE_A; break;
        case 'g': case 'G': b = BASE_G; break;
        case 't': case 'T': b = BASE_T; break;
        case 'c': case 'C': b = BASE_C; break;
        // Ignore all non-base characters.
        default:
            continue;
        }
        // Necessary so we can move the resize to outside of `strand_set()`
        dprintf("[i=%2" PRIu64 ", ", s.base_count);
        i = strand_index_make(s.base_count++);
        list_resize(LIMB_TYPE, &s.limbs, i.limb_index + 1);
        strand_set(&s, i, b);
    }
    return s;
}

static void
limb_print(LIMB_TYPE limb, LIMB_TYPE bit_count)
{
    dprintf("\t\t%" PRIu32 ", // %s, \"", limb, binle(limb));
    for (LIMB_TYPE bit = 0; bit < bit_count; bit += BASE_BITS) {
        // if (bit > 0 && (bit % 3) == 0) {
        //     putchar(' ');
        // }
        putchar(BASE_CHARS[(limb >> bit) & BASE_MAX]);
    }
    dprintf("\"\n");
}

static void
strand_print(const Strand *s)
{
    LIMB_TYPE limb, count;
    u64 i, n;

    // Avoid infinite loops from overflow in subtraction.
    // We should never have strands with limb counts of 0 anyway.
    assert(s->limbs.len > 0);
    assert(s->base_count > 0);
    dprintf("Strand{\n"
           "\tlimbs={\n");
    for (i = 0, n = s->limbs.len - 1; i < n; i += 1) {
        limb = s->limbs.data[i];
        limb_print(limb, LIMB_TYPE_BITS);
    }

    // Last limb is a special case as not all its bits may be used.
    limb  = s->limbs.data[i];

    // Count how many bases we use for the last limb specifically.
    // Modulo by bit size breaks here once `limbs.len` >= 2.
    count = cast(LIMB_TYPE)(s->base_count - (n * LIMB_TYPE_BASES));
    limb_print(limb, count * BASE_BITS);
    dprintf("\t},\n"
           "\tlimbs.len=%" PRIu64 ",\n"
           "\tbase_count=%" PRIu64 ",\n"
           "}",
           s->limbs.len, s->base_count);
}


static void
strand_destroy(Strand *s)
{
    list_delete(&s->limbs);
    s->base_count = 0;
}

static String
string_make_cstring(const char *cstring)
{
    String s = {cstring, strlen(cstring)};
    return s;
}

int
main(int argc, char *argv[])
{
    String dna = {NULL, 0};
    if (argc == 2) {
        dna = string_make_cstring(argv[1]);
    } else {
        fprintf(stderr, "[USAGE] %s <dna>\n", argv[0]);
        return 1;
    }
    Strand s = strand_make(dna);
    strand_print(&s);
    putchar('\n');
    strand_destroy(&s);
    return 0;
}
