#include <assert.h> // assert
#include <limits.h> // CHAR_BIT
#include <stdbool.h> // bool
#include <stdint.h> // u?int[\d]+_t
#include <stdio.h>  // printf family
#include <stdlib.h> // malloc, realloc, free
#include <string.h> // strlen

#define BIT_SIZE(T)     (sizeof(T) * CHAR_BIT)
#define cast(T)         (T)
#define unused(expr)    cast(void)(expr)

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

// Ordered in such a way that getting the complements is as easy
// as doing a bitwise XOR on all limbs.
enum Base {
    BASE_A, // 0b00
    BASE_G, // 0b01
    BASE_C, // 0b10
    BASE_T, // 0b11
};

typedef struct String String;
struct String {
    const char *data;
    size_t len;
};

// A 1D array of bases. It is actually a bit array, so the bases are
// very tightly packed.
typedef struct Strand Strand;
struct Strand {
    // Each limb can fit (`bit_size / 2`) bases.
    // I.e. a 32-bit int can fit 16 bases.
    //
    // A DNA sequence, with length in range [[1,32]], uses 1 limb.
    // A DNA sequence, with length in range [[33,64]], uses 2 limbs.
    u32 *limbs;

    // The number of limbs we allocated for `limbs`.
    u64 limb_count;

    // The longest genome known as of 2026 is a fern, Tmesipteris oblanceolate.
    // It has 160 billion base pairs so this must be of a suitably-sized type.
    //
    // @link https://www.science.org/content/article/unassuming-fern-has-largest-known-genome-and-no-one-knows-why
    u64 base_count;
};

#define LIST_FIELDS(T)                                                         \
    T *data;                                                                   \
    size_t len, cap                                                            \

typedef struct Strand_List Strand_List;
struct String_List {
    LIST_FIELDS(String);
};

typedef struct Strand_List Strand_List;
struct Strand_List {
    LIST_FIELDS(Strand);
};

// Pseudo-methods
#define list_init(list)                                                        \
do {                                                                           \
    (list)->data = NULL;                                                       \
    (list)->len  = 0;                                                          \
    (list)->cap  = 0;                                                          \
} while (0)

// Reserve `new_cap` number of elements.
// `list->cap` is set but not `list->len`.
#define list_reserve(T, list, new_cap)                                         \
do {                                                                           \
    size_t _new_cap = (new_cap);                                               \
    T *_new_data = realloc((list)->data, _new_cap);                            \
    if (_new_data == NULL) {                                                   \
        assert(false && "Buy more RAM lol");                                   \
    }                                                                          \
    (list)->data = _new_data;                                                  \
    (list)->cap  = _new_cap;                                                   \
} while (0)

// Write `item` to the first free slot in the list, resizing it if needed.
#define list_append(T, list, item)                                             \
do {                                                                           \
    size_t _cap = (list)->cap;                                                 \
    if ((list)->len + 1 > _cap) {                                              \
        list_reserve(T, list, (_cap > 8) ? _cap * 2 : 8);                      \
    }                                                                          \
    (list)->data[(list)->len++] = item;                                        \
} while (0)

#define list_delete(list)                                                      \
do {                                                                           \
    free((list)->data);                                                        \
} while (0)

static void
strand_set(Strand *s, u64 index, enum Base base)
{
    const u64 bits = BIT_SIZE(s->limbs[0]);

    // Get 0-based index of the limb which contains `index`.
    // Note that unsigned integer division always returns the floor.
    //
    // I.e.  indexes in the range [0,32)  give us limb index 0,
    // while indexes in the range [32,64) give us limb index 1,
    // and   indexes in the range [64,96) give us limb index 2.
    u64 limb_index = index / bits;

    // Get the index of the bit within the limb we want to manipulate.
    // Note that since bases are 2-bits, our bit indexes are likewise
    // always multiples of 2.
    //
    // I.e. index == 0 should give us 0,
    // but  index == 1 should give us 2 since the 0th base occupies [0, 1].
    // and  index == 3 should give us 4 since indexes [0, 3] are used.
    u32 bit_index = cast(u32)((index % bits) + (index % 2));
    u32 *limb = &s->limbs[limb_index];
    unused(base);
    unused(bit_index);
    unused(limb);
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
    y = BIT_SIZE(s.limbs[0]);
    q = (x + y - 1) / y;
    s.limb_count = q;

    s.limbs = cast(u32 *)calloc(s.limb_count, sizeof(s.limbs[0]));
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
    strand_destroy(&s);
    return 0;
}
