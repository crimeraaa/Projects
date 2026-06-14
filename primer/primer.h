#ifndef PRIMER_H
#define PRIMER_H

// standard
#include <assert.h> // assert
#include <limits.h> // CHAR_BIT
#include <stdint.h> // u?int[\d]+_t
#include <stdio.h>  // printf family
#include <stdlib.h> // malloc, realloc, free
#include <string.h> // strlen

// local
#include "list.h"

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

#define BASE_CHARS  "AGCT"
#define BASE_BITS   2
#define BASE_MAX    BASE_T

typedef struct String String;
struct String {
    const char *data;
    size_t len;
};

#define LIMB_TYPE   u32
#define LIMB_SIZE   sizeof(LIMB_TYPE)
#define LIMB_BITS   bit_size(LIMB_TYPE)
#define LIMB_BASES  (LIMB_BITS / BASE_BITS)

// A 1D array of bases. It is actually a bit array, so the bases are
// very tightly packed.
typedef struct Strand Strand;
struct Strand {
    // Each limb can fit (`bit_size / BASE_BITS`) bases.
    // I.e. a 32-bit int can fit 16 bases.
    //
    // A DNA sequence, with length in range [[1,32]], uses 1 limb.
    // A DNA sequence, with length in range [[33,64]], uses 2 limbs.
    LIST_FIELDS(LIMB_TYPE);

    // Since each limb can fit multiple bases, the actual base count must
    // be tracked separately. This is necessary especially in the case of
    // the last limb which may not use all its bits.
    //
    // The longest genome known as of 2026 is a fern, Tmesipteris oblanceolate.
    // It has 160 billion base pairs so this must be of a suitably-sized type.
    //
    // @link https://www.science.org/content/article/unassuming-fern-has-largest-known-genome-and-no-one-knows-why
    u64 base_count;
};


#endif // !PRIMER_H
