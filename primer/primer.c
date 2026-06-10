// standard
#include <inttypes.h> // PRI* macros

// local
#include "primer.h"
#include "list.h"

// Modulo where `y`, the divisor, is a power of 2.
#define mod2(x, y)  ((x) & ((y) - 1))

static void
string_list_reserve(String_List *list, size_t new_cap)
{
    list_reserve(String, list, new_cap);
}

static void
strand_list_reserve(Strand_List *list, size_t new_cap)
{
    list_reserve(Strand, list, new_cap);
}

// Debug only
static const char *
bin(u32 value)
{
    static char buf[BIT_SIZE(value) + 1];
    u32 i, n, bit, start, stop;

    // Print MSB to LSB.
    start = 0;
    stop  = sizeof(buf) - 1;
    for (n = start; n < stop; n += 1) {
        // We want to put LSB at the last non-nul index.
        // As we iterate, we effectively go backwards.
        i   = stop - 1 - n;
        bit = value >> n;
        buf[i] = (bit & 1) ? '1' : '0';
    }
    return buf;
}

static void
strand_set(Strand *s, u64 index, enum Base base)
{
    LIMB_TYPE *limb_ptr, bit_index, mask;
    u64 limb_index;

    // Get 0-based index of the limb which contains `index`.
    // Note that unsigned integer division always returns the floor.
    //
    // I.e.  indexes in the range [0,32)  give us limb index 0,
    // while indexes in the range [32,64) give us limb index 1,
    // and   indexes in the range [64,96) give us limb index 2.
    limb_index = index / LIMB_TYPE_BITS;

    // Get the index of the bit within the limb we want to manipulate.
    // Note that since bases are 2-bits, our bit indexes are likewise
    // always multiples of 2.
    //
    // I.e. index == 0 should give us 0,
    // but  index == 1 should give us 2 since the 0th base occupies [0, 1].
    // and  index == 3 should give us 4 since indexes [0, 3] are used.
    bit_index  = cast(u32)(mod2(index, LIMB_TYPE_BITS) + mod2(index, BASE_BITS));
    limb_ptr   = &s->limbs[limb_index];

    // We'll need to clear out 2-bit value at this bit index before
    // setting in the new value given by `base`.
    mask      = ~(BASE_MAX << bit_index);
    *limb_ptr = (*limb_ptr & mask) | (cast(LIMB_TYPE)base << bit_index);

    printf("[%u, %u] = %u // 0b%s\n",
           bit_index, bit_index + 1, *limb_ptr, bin(*limb_ptr));
}

// Ignore all non-base characters.
static u64
count_bases(String dna)
{
    u64 count = 0;
    for (const char *it = dna.data, *end = it + dna.len; it < end; it += 1) {
        switch (*it) {
        case 'a': case 'A':
        case 'g': case 'G':
        case 't': case 'T':
        case 'c': case 'C':
            count += 1;
            break;
        }
    }
    return count;
}

static Strand
strand_make(String dna)
{
    Strand s = {NULL, 0, 0};
    s.base_count = count_bases(dna);

    // Integer ceiling division so that 32 / 32 = 1, but 33 / 32 = 2.
    // It can be achieved by the following formula:
    //
    //  q = (x + y - 1) / y`
    //
    // If you are worried about overflow in `y`, and `x != 0`, then you
    // can also do:
    //
    //  `q = 1 + ((x - 1) / y)`
    u64 q, x, y;
    x = s.base_count;
    y = LIMB_TYPE_BITS;
    q = (x + y - 1) / y;
    s.limb_count = q;
    s.limbs      = cast(LIMB_TYPE *)calloc(s.limb_count, sizeof(s.limbs[0]));
    if (s.limbs == NULL) {
        assert(false && "Buy more RAM lol");
    }

    u64 i = 0;
    for (const char *it = dna.data, *end = it + dna.len; it < end; it += 1) {
        switch (*it) {
        case 'a': case 'A': strand_set(&s, i++, BASE_A); break;
        case 'g': case 'G': strand_set(&s, i++, BASE_G); break;
        case 't': case 'T': strand_set(&s, i++, BASE_T); break;
        case 'c': case 'C': strand_set(&s, i++, BASE_C); break;
        }
    }
    return s;
}

static void
limb_print(LIMB_TYPE limb, LIMB_TYPE bit_count)
{
    printf("\t\t%" PRIu32 ", // 0b%s, \"", limb, bin(limb));
    for (LIMB_TYPE bit = 0; bit < bit_count; bit += BASE_BITS) {
        enum Base base = cast(enum Base)((limb >> bit) & BASE_MAX);
        putchar(BASE_CHARS[base]);
    }
    printf("\"\n");
}

static void
strand_print(const Strand *s)
{
    LIMB_TYPE limb, count;

    // Avoid infinite loops from overflow in subtraction.
    // We should never have strands with limb counts of 0 anyway.
    assert(s->limb_count > 0);
    printf("Strand{\n"
           "\tlimbs={\n");
    for (size_t i = 0, n = s->limb_count - 1; i < n; i += 1) {
        limb = s->limbs[i];
        limb_print(limb, LIMB_TYPE_BITS);
    }

    // Last limb is a special case as not all its bits may be used.
    // Modulo returns the number of bits used for the last limb.
    limb  = s->limbs[s->limb_count - 1];
    count = cast(LIMB_TYPE)mod2(s->base_count, LIMB_TYPE_BITS);
    limb_print(limb, count * BASE_BITS);
    printf("\t},\n"
           "\tlimb_count=%" PRIu64 ",\n"
           "\tbase_count=%" PRIu64 ",\n"
           "}\n",
           s->limb_count, s->base_count);
}


static void
strand_destroy(Strand *s)
{
    free(s->limbs);
    s->limbs      = NULL;
    s->limb_count = 0;
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
    strand_destroy(&s);
    return 0;
}
