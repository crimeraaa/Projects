#ifndef LULU_STRINGS_H
#define LULU_STRINGS_H

#include <inttypes.h>

#include "internal.h"

/*
 NIT(2026-07-03):
    One of the few cases I would love to have C++ style templates.
 */
typedef struct String String;
struct String {
    const char *data;
    usize       len;
};

#define string_literal(s)   {s, sizeof(s) - 1}

static inline String
string_make(const char *data, usize len);

static inline String
string_make(const char *data, usize len)
{
    String s = {data, len};
    return s;
}

static inline String
string_make_cstring(const char *s)
{
    usize n = 0;
    while (s[n] != 0) {
        n++;
    }
    return string_make(s, n);
}

static inline String
string_slice(String s, usize start, usize stop)
{
    String s2 = {s.data + start, stop - start};
    return s2;
}

static inline String
string_slice_from(String s, usize start)
{
    return string_slice(s, start, s.len);
}

#define FNV32A_OFFSET  0x811c9dc5
#define FNV32A_PRIME   0x01000193

// TODO(2026-07-02): Make this more robust? Use vectors when possible?
#define WORD            u64
#define WORD_SIZE       8
#define WORD_ALIGNED(p) ((cast(uintptr)p & (WORD_SIZE - 1)) == 0)

static inline u32
string_hash(String s)
{
    const char *p    = s.data;
    const char *end  = p + s.len;
    u32         hash = FNV32A_OFFSET;
    u32         data = 0;

    // Hash byte-by-byte while our pointer is not word-aligned.
    for (; p < end && !WORD_ALIGNED(p); p++) {
        hash = (hash ^ cast(u32)*p) * FNV32A_PRIME;
    }

    // Hash word-by-word to reduce the number of memory reads.
    for (; p + WORD_SIZE <= end; p += WORD_SIZE) {
        WORD c = *cast(WORD *)p;
        hash = (hash ^ cast(u32)((c >>  0) & 0xff)) * FNV32A_PRIME;
#if WORD_SIZE > 1 // ^^^  8-bit; vvv 16-bit
        hash = (hash ^ cast(u32)((c >>  8) & 0xff)) * FNV32A_PRIME;
#if WORD_SIZE > 2 // ^^^ 16-bit; vvv 32-bit
        hash = (hash ^ cast(u32)((c >> 16) & 0xff)) * FNV32A_PRIME;
        hash = (hash ^ cast(u32)((c >> 24) & 0xff)) * FNV32A_PRIME;
#if WORD_SIZE > 4 // ^^^ 32-bit; vvv 64-bit
        hash = (hash ^ cast(u32)((c >> 32) & 0xff)) * FNV32A_PRIME;
        hash = (hash ^ cast(u32)((c >> 40) & 0xff)) * FNV32A_PRIME;
        hash = (hash ^ cast(u32)((c >> 48) & 0xff)) * FNV32A_PRIME;
        hash = (hash ^ cast(u32)((c >> 56) & 0xff)) * FNV32A_PRIME;
#endif // WORD_SIZE > 4
#endif // WORD_SIZE > 2
#endif // WORD_SIZE > 1
    }

    // Hash the Remainder of string that doesn't fit in a word.
    for (; p < end; p++) {
        hash = (hash ^ cast(u32)*p) * FNV32A_PRIME;
    }
    return hash;
}

#undef WORD_ALIGNED
#undef WORD_SIZE
#undef WORD
#undef FNV32A_PRIME
#undef FNV32A_OFFSET

#endif // !LULU_STRINGS_H
