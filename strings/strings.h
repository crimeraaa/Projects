#ifndef STRINGS_H
#define STRINGS_H

#ifndef STRINGS_DEF
#define STRINGS_DEF
#endif

// local
#include "../common.h"

// We assume that this is never a valid string length.
// For a 64-bit system, an 18-quintillion byte string should be impossible.
#define STRING_NOT_FOUND    SIZE_MAX

// Read-only views into character arrays.
typedef struct string_View string_View;
struct string_View {
    const char *data;
    size_t len;
};

STRINGS_DEF inline string_View
string_make(const char *s, size_t n)
{
    string_View sv = {s, n};
    return sv;
}

STRINGS_DEF inline string_View
string_make_cstring(const char *s)
{
    size_t n = 0;
    // Manual strlen.
    while (s[n] != 0) {
        n++;
    }
    return string_make(s, n);
}

STRINGS_DEF inline bool
string_eq(string_View a, string_View b)
{
    if (a.len != b.len) {
        return false;
    }

    // Same length and same pointer?
    if (a.data == b.data) {
        return true;
    }

    // Manual `memcmp`. Could possibly be vectorized for larger strings.
    for (size_t i = 0; i < a.len; i++) {
        // Unlike `memcmp()`, we don't care about the ordering.
        if (a.data[i] != b.data[i]) {
            return false;
        }
    }
    return true;
}


// Equivalent to `s[start:stop]` in languages like Python and Odin.
STRINGS_DEF inline string_View
string_slice(string_View s, size_t start, size_t stop)
{
    // If we use signed sizes, we'll also need to check against 0. We are
    // allowed to point to 1 past the last character, we just can't dereference
    // it. This is useful to slice a string into the empty string (e.g.
    // eliminating '-' from number strings).
    assert(start <= s.len);

    // Ensure subtraction results in valid values.
    assert(start <= stop && stop <= s.len);
    return string_make(s.data + start, stop - start);
}

// Equivalent to `s[start:]` in langauges like Python and Odin.
STRINGS_DEF inline string_View
string_slice_from(string_View s, size_t start)
{
    return string_slice(s, start, s.len);
}

// Equivalent to `s[:stop]` in languages like Python and Odin.
STRINGS_DEF inline string_View
string_slice_until(string_View s, size_t stop)
{
    return string_slice(s, 0, stop);
}

// Returns the first index of `c` in `s`, or `STRING_NOT_FOUND`.
STRINGS_DEF size_t
string_index_char(string_View s, char c);

// Returns the last index of `c` in `s`, or `STRING_NOT_FOUND`.
STRINGS_DEF size_t
string_last_index_char(string_View s, char c);

// Returns the first index of the needle `p` (a substring) in the haystack `s`.
// Returns `STRING_NOT_FOUND` otherwise.
STRINGS_DEF size_t
string_index(string_View s, string_View p);

// Returns the last index of the needle `p` (a substring) in the haystack `s`.
// Returns `STRING_NOT_FOUND` ohterwise.
STRINGS_DEF size_t
string_last_index(string_View s, string_View p);

STRINGS_DEF string_View
string_trim(string_View s);

// The string creation and slicing functions are simple enough to inline.
// The find functions, however, are not.
// #define STRINGS_IMPLEMENTATION
#ifdef STRINGS_IMPLEMENTATION

#include "../ascii/ascii.h"

STRINGS_DEF size_t
string_index_char(string_View s, char c)
{
    for (size_t i = 0; i < s.len; i += 1) {
        if (s.data[i] == c) {
            return i;
        }
    }
    return STRING_NOT_FOUND;
}

STRINGS_DEF size_t
string_last_index_char(string_View s, char c)
{
    // Reverse iteration with unsigned types sucks
    for (size_t i = s.len; i-- > 0;) {
        if (s.data[i] == c) {
            return i;
        }
    }
    return STRING_NOT_FOUND;
}

STRINGS_DEF size_t
string_index(string_View s, string_View p)
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

        string_View sub = string_slice(s, start, stop);
        if (string_eq(sub, p)) {
            return start;
        }
    }
    return STRING_NOT_FOUND;
}

STRINGS_DEF size_t
string_last_index(string_View s, string_View p)
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
        string_View sub = string_slice(s, start, start + p.len);
        if (string_eq(sub, p)) {
            return start;
        }
    }
    return STRING_NOT_FOUND;
}

STRINGS_DEF string_View
string_trim(string_View s)
{
    size_t start, stop;
    // Trim left
    for (start = 0; start < s.len; start++) {
        if (!ascii_is_space(s.data[start])) {
            break;
        }
    }

    // Trim right
    for (stop = s.len; stop > 0; stop--) {
        if (!ascii_is_space(s.data[stop - 1])) {
            break;
        }
    }
    return string_slice(s, start, stop);
}

#endif // STRINGS_IMPLEMENTATION
#endif // !STRINGS_H
