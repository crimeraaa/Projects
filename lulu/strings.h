#ifndef LULU_STRINGS_H
#define LULU_STRINGS_H

// standard
#include <string.h> // memcmp

// local
#include "internal.h"

/*
 NIT(2026-07-03):
    One of the few cases I would love to have C++ style templates.
 */
typedef struct String String;
struct String {
    char const *data;
    usize       len;
};

#define string_literal(s)   {s, sizeof(s) - 1}

static inline String
string_make(char const *data, usize len)
{
    String s = {data, len};
    return s;
}

static inline String
string_make_cstring(char const *s)
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

static inline bool
string_eq(String a, String b)
{
    return a.len == b.len && memcmp(a.data, b.data, b.len) == 0;
}

#define FNV32A_OFFSET  0x811c9dc5
#define FNV32A_PRIME   0x01000193

static inline u32
string_hash(String s)
{
    char const *      p   = s.data;
    char const *const end = p + s.len;

    u32 hash = FNV32A_OFFSET;
    while (p < end) {
        hash = (hash ^ cast(u32)*p++) * FNV32A_PRIME;
    }
    return hash;
}

#endif // !LULU_STRINGS_H
