#ifndef STRINGS_H
#define STRINGS_H

// standard
#include <string.h>     // memcmp, strlen

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

internal inline String
string_make(const char *s, size_t n)
{
    String s2 = {s, n};
    return s2;
}

internal inline String
string_make_cstring(const char *s)
{
    String s2 = {s, strlen(s)};
    return s2;
}

internal inline bool
string_eq(String a, String b)
{
    if (a.len == b.len) {
        return memcmp(a.data, b.data, b.len) == 0;
    }
    return false;
}


// Equivalent to `s[start:stop]` in languages like Python and Odin.
internal inline String
string_slice(String s, size_t start, size_t stop)
{
    // If we use signed sizes, we'll also need to check against 0.
    assert(start < s.len);
    // Start must be <= stop in order for subtraction to be valid.
    assert(start <= stop && stop <= s.len);
    return string_make(&s.data[start], stop - start);
}

// Equivalent to `s[start:]` in langauges like Python and Odin.
internal inline String
string_slice_from(String s, size_t start)
{
    return string_slice(s, start, s.len);
}

// Equivalent to `s[:stop]` in languages like Python and Odin.
internal inline String
string_slice_until(String s, size_t stop)
{
    return string_slice(s, 0, stop);
}

// Returns the first index of `c` in `s`, or `STRING_NOT_FOUND`.
global size_t
string_index_char(String s, char c);

// Returns the last index of `c` in `s`, or `STRING_NOT_FOUND`.
global size_t
string_last_index_char(String s, char c);

// Returns the first index of the needle `p` in the haystack `s`.
// Returns `STRING_NOT_FOUND` otherwise.
global size_t
string_index(String s, String p);

// Returns the last index of the needle `p` in the haystack `s`.
// Returns `STRING_NOT_FOUND` ohterwise.
global size_t
string_last_index(String s, String p);

#ifdef STRINGS_IMPLEMENTATION

global size_t
string_index_char(String s, char c)
{
    for (size_t i = 0; i < s.len; i += 1) {
        if (s.data[i] == c) {
            return i;
        }
    }
    return STRING_NOT_FOUND;
}

global size_t
string_last_index_char(String s, char c)
{
    // Reverse iteration with unsigned types sucks
    for (size_t i = s.len; i-- > 0;) {
        if (s.data[i] == c) {
            return i;
        }
    }
    return STRING_NOT_FOUND;
}

global size_t
string_index(String s, String p)
{
    switch (p.len) {
    // The empty string is always present at the start.
    case 0: return 0;
    case 1: return string_index_char(s, p.data[0]);
    default:
        if (s.len == p.len) {
            return string_eq(s, p) ? 0 : STRING_NOT_FOUND;
        } else if (s.len < p.len) {
            return STRING_NOT_FOUND;
        }
    }

    // O(mn) algorithm is atrocious
    for (size_t start = 0; start < s.len; start++) {
        size_t stop = start + p.len;
        // Substring couldn't possibly be found from here on?
        if (stop > s.len) {
            break;
        }

        String sub = string_slice(s, start, stop);
        if (string_eq(sub, p)) {
            return start;
        }
    }
    return STRING_NOT_FOUND;
}

global size_t
string_last_index(String s, String p)
{
    switch (p.len) {
    // The empty string is always present at the start.
    case 0: return 0;
    case 1: return string_last_index_char(s, p.data[0]);
    default:
        if (s.len == p.len) {
            return string_eq(s, p) ? 0 : STRING_NOT_FOUND;
        } else if (s.len < p.len) {
            return STRING_NOT_FOUND;
        }
    }

    // Reverse iteration with unsigned types sucks
    // Also this O(mn) algorithm is atrocious...
    for (size_t start = s.len - p.len; start-- > 0;) {
        String sub = string_slice(s, start, start + p.len);
        if (string_eq(sub, p)) {
            return start;
        }
    }
    return STRING_NOT_FOUND;
}

#endif // STRINGS_IMPLEMENTATION

#endif // !STRINGS_H
