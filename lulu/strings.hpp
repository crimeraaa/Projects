#pragma once

// standard
#include <cstring> // memcmp

// local
#include "internal.hpp"
#include "slice.hpp"

using String = Slice<char const>;

static inline String constexpr
operator""_s(char const *literal, usize literal_len)
{
    return {literal, literal_len};
}

static inline String
string_make_cstring(char const *cstring)
{
    usize cstring_len = 0;
    while (cstring[cstring_len] != 0) {
        cstring_len++;
    }
    return {cstring, cstring_len};
}

static inline bool
operator==(String a, String b)
{
    return len(a) == len(b) && std::memcmp(a.data, b.data, b.len) == 0;
}

#define FNV32A_OFFSET  0x811c9dc5
#define FNV32A_PRIME   0x01000193

static inline u32
string_hash(String s)
{
    u32 hash = FNV32A_OFFSET;
    for (char c : s) {
        hash = (hash ^ cast(u32)c) * FNV32A_PRIME;
    }
    return hash;
}

/*
 For use with printf-like functions that accept the `%.*s` specifiers.
 */
#define STRING_EXPAND(s)    (cast(int)(s).len), ((s).data)

