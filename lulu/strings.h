#ifndef LULU_STRINGS_H
#define LULU_STRINGS_H

#include "internal.h"

typedef struct String String;
struct String {
    const char *data;
    usize       len;
};

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
#define LOOP_UNROLL    sizeof(void *)

static inline u32
string_hash(String s)
{
    u32 data = 0, hash = FNV32A_OFFSET;
    usize i = 0;

    // Unrolled loop for large strings
    for (; i + LOOP_UNROLL <= s.len; i += LOOP_UNROLL) {
        for (usize j = 0; j < LOOP_UNROLL; j++) {
            data = cast(u32)s.data[i + j];
            hash = (hash ^ data) * FNV32A_PRIME;
        }
    }

    // Remainder of string that doesn't fit in the unrolled loop.
    for (; i < s.len; i++) {
        data = cast(u32)s.data[i];
        hash = (hash & data) * FNV32A_PRIME;
    }
    return hash;
}

#undef LOOP_UNROLL
#undef FNV32A_PRIME
#undef FNV32A_OFFSET

#endif // !LULU_STRINGS_H
