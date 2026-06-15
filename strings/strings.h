#ifndef STRINGS_H
#define STRINGS_H

// standard
#include <string.h>

// local
#include "../common.h"

// We assume that this is never a valid string length.
// For a 64-bit system, an 18-quintillion byte string should be impossible.
#define STRING_NOT_FOUND    SIZE_MAX

// Strings are read-only views into character arrays.
typedef struct String String;
struct String {
    const char *data;
    size_t len;
};

static inline String
string_make(const char *s, size_t n)
{
    String s2 = {s, n};
    return s2;
}

static inline bool
string_eq(String a, String b)
{
    if (a.len == b.len) {
        return memcmp(a.data, b.data, b.len) == 0;
    }
    return false;
}


// Equivalent to `s[start:stop]` in languages like Python and Odin.
static inline String
string_slice(String s, size_t start, size_t stop)
{
    // If we use signed sizes, we'll also need to check against 0.
    assert(start < s.len);
    // Start must be <= stop in order for subtraction to be valid.
    assert(start <= stop && stop <= s.len);
    return string_make(&s.data[start], stop - start);
}

// Equivalent to `s[start:]` in langauges like Python and Odin.
static inline String
string_slice_from(String s, size_t start)
{
    return string_slice(s, start, s.len);
}

// Equivalent to `s[:stop]` in languages like Python and Odin.
static inline String
string_slice_until(String s, size_t stop)
{
    return string_slice(s, 0, stop);
}

// Returns the first index of `c` in `s`, or `STRING_NOT_FOUND` on failure.
static inline size_t
string_index_char(String s, char c)
{
    for (size_t i = 0; i < s.len; i += 1) {
        if (s.data[i] == c) {
            return i;
        }
    }
    return STRING_NOT_FOUND;
}

// Returns the first index of `p` in `s`, or `STRING_NOT_FOUND` on failure.
static inline size_t
string_index_string(String s, String p)
{
    for (size_t start = 0, stop = p.len;
        start < s.len && stop <= s.len;
        start++, stop++)
    {
        String sub = string_slice(s, start, stop);
        if (string_eq(sub, p)) {
            return start;
        }
    }
    return STRING_NOT_FOUND;
}

#endif // !STRINGS_H
