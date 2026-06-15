// standard
#include <inttypes.h> // PRI* macros

// local
#define MEM_ARENA_IMPLEMENTATION
#include "../mem/arena.h"
#include "primer.h"


// Modulo where `y`, the divisor, is a power of 2.
#define mod2(x, y)  ((x) & ((y) - 1))

#if 0
#define dprintf(...)    printf(__VA_ARGS__)

internal const char *
binle(LIMB_TYPE value)
{
    local_persist char buf[bit_size(value) + 1];
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
    u64 limb, bit;
};

internal Strand_Index
strand_index_make(u64 base_index)
{
    Strand_Index i = {0, 0};

    // Get 0-based index of the limb which contains `index`.
    // Note that unsigned integer division always returns the floor.
    //
    // I.e.  indexes in the range [0,32)  give us limb index 0,
    // while indexes in the range [32,64) give us limb index 1,
    // and   indexes in the range [64,96) give us limb index 2.
    i.limb = (base_index * BASE_BITS) / LIMB_BITS,

    // Get the index of the bit within the limb we want to manipulate.
    // Note that since bases are 2-bits, our bit indexes are likewise
    // always multiples of 2.
    //
    // I.e. index == 0 should give us 0,
    // but  index == 1 should give us 2 since the 0th base occupies [0, 1].
    // and  index == 3 should give us 4 since indexes [0, 3] are used.
    i.bit = cast(LIMB_TYPE)mod2(base_index * BASE_BITS, LIMB_BITS);
    return i;
}

internal void
strand_set(Strand *s, Strand_Index index, enum Base base)
{
    LIMB_TYPE *limb_ptr, mask;
    limb_ptr  = &s->data[index.limb];

    // Clear out 2-bit value at this bit index before setting in the new value
    // as given by `base`.
    mask      = ~(BASE_MAX << index.bit);
    *limb_ptr = (*limb_ptr & mask) | (cast(LIMB_TYPE)base << index.bit);

    dprintf("l=%" PRIu64  ", b=%02" PRIu64 ":%02" PRIu64 "] = %10u // %s\n",
            index.limb, index.bit, index.bit + 2, *limb_ptr, binle(*limb_ptr));
}

internal Strand
strand_make(String dna, mem_Allocator allocator)
{
    Strand s = {NULL, 0, 0};
    for (const char *it = dna.data, *end = it + dna.len; it < end; it += 1) {
        Strand_Index index;
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
        index = strand_index_make(s.base_count++);
        list_resize(LIMB_TYPE, &s, index.limb + 1, allocator);
        strand_set(&s, index, b);
    }
    return s;
}

internal void
strand_limb_print(LIMB_TYPE limb, LIMB_TYPE bit_count)
{
    dprintf("\t\t%" PRIu32 ", // %s, \"", limb, binle(limb));
    for (LIMB_TYPE bit = 0; bit < bit_count; bit += BASE_BITS) {
#if 0
        if (bit > 0 && (bit % 3) == 0) {
            putchar(' ');
        }
#endif
        putchar(BASE_CHARS[(limb >> bit) & BASE_MAX]);
    }
    dprintf("\"\n");
}

internal void
strand_print(const Strand *s)
{
    LIMB_TYPE limb, count;
    size_t i, n;

    // Avoid infinite loops from overflow in subtraction.
    // We should never have strands with limb counts of 0 anyway.
    assert(s->len > 0);
    assert(s->base_count > 0);
    dprintf("Strand{\n"
           "\tdata={\n");
    for (i = 0, n = s->len - 1; i < n; i += 1) {
        limb = s->data[i];
        strand_limb_print(limb, LIMB_BITS);
    }

    // Last limb is a special case as not all its bits may be used.
    limb = s->data[i];

    // Count how many bases we use for the last limb specifically.
    // We assume that all bases before the last one use all their bits.
    count = cast(LIMB_TYPE)(s->base_count - (cast(u64)n * LIMB_BASES));
    strand_limb_print(limb, count * BASE_BITS);
    dprintf("\t},\n"
           "\tlen=%zu,\n"
           "\tcap=%zu,\n"
           "\tbase_count=%" PRIu64 ",\n"
           "}",
           s->len, s->cap, s->base_count);
}

internal void
strand_destroy(Strand *s, mem_Allocator allocator)
{
    list_delete(s, allocator);
    s->base_count = 0;
}

internal String
string_make_cstring(const char *cstring)
{
    String s = {cstring, strlen(cstring)};
    return s;
}

int
main(int argc, char *argv[])
{
    String dna = {NULL, 0};
    Strand s;
    mem_Allocator allocator;
    mem_Arena arena;
    char buf[8192];
    if (argc == 2) {
        dna = string_make_cstring(argv[1]);
    } else {
        fprintf(stderr, "[USAGE] %s <dna>\n", argv[0]);
        return 1;
    }

    mem_arena_init(&arena, buf, sizeof(buf));
    allocator = mem_arena_allocator(&arena);

    s = strand_make(dna, allocator);
    strand_print(&s);
    putchar('\n');
    strand_destroy(&s, allocator);
    return 0;
}
