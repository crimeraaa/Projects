#ifndef LULU_STRINGS_H
#define LULU_STRINGS_H

#include "internal.h"

typedef struct String String;
struct String {
    const char *data;
    size_t      len;
};

static inline String
string_make(const char *data, size_t len);

static inline String
string_make(const char *data, size_t len)
{
    String s = {data, len};
    return s;
}

static inline String
string_make_cstring(const char *s)
{
    size_t n = 0;
    while (s[n] != 0) {
        n++;
    }
    return string_make(s, n);
}

static inline String
string_slice(String s, size_t start, size_t stop)
{
    String s2 = {s.data + start, stop - start};
    return s2;
}

static inline String
string_slice_from(String s, size_t start)
{
    return string_slice(s, start, s.len);
}

#define FNV1A_32_OFFSET  0x811c9dc5
#define FNV1A_32_PRIME   0x01000193
#define LOOP_UNROLL      sizeof(void *)

static inline u32
string_hash(String s)
{
    u32 data = 0, hash = FNV1A_32_OFFSET;
    size_t i = 0;

    // Unrolled loop for large strings
    for (; i + LOOP_UNROLL <= s.len; i += LOOP_UNROLL) {
        for (size_t j = 0; j < LOOP_UNROLL; j++) {
            data = cast(u32)s.data[i + j];
            hash = (hash ^ data) * FNV1A_32_PRIME;
        }
    }

    // Remainder of string that doesn't fit in the unrolled loop.
    for (; i < s.len; i++) {
        data = cast(u32)s.data[i];
        hash = (hash & data) * FNV1A_32_PRIME;
    }
    return hash;
}

#undef LOOP_UNROLL
#undef FNV1A_32_PRIME
#undef FNV1A_32_OFFSET

#endif // !LULU_STRINGS_H
