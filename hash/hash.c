#include <stdbool.h> // bool, true, false
#include <stddef.h>  // size_t
#include <stdint.h>  // uint64_t
#include <stdio.h>   // fprintf, printf

#define cast(T) (T)

bool
string_find_char(char const *s, size_t n, char c, size_t *out_i)
{
    for (size_t i = 0; i < n; i++) {
        if (s[i] == c) {
            if (out_i) {
                *out_i = i;
            }
            return true;
        }
    }
    return false;
}

bool
string_eq_unsafe(char const *a, char const *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

bool
string_eq(char const *a, size_t a_len, char const *b, size_t b_len)
{
    return a_len == b_len && string_eq_unsafe(a, b, b_len);
}

#define HASH_PRIME  16777619U

static uint32_t
rabin_karp_hash(char const *s, size_t n)
{
    uint32_t hash  = 0;
    for (size_t i = 0; i < n; i++) {
        // Multiplying the previous result by the base 'pushes up'
        // the previous digits by one place value. This is the same as
        // multiplying the current character by the largest unoccupied
        // place value and decrementing the exponent.
        hash = hash * HASH_PRIME + cast(uint32_t)s[i];
    }
    return hash;
}

/// Description:
///     Calculates the maximum place value needed to 'chop off'
///     digits that are leaving the window.
static uint32_t
rabin_karp_pow(size_t n)
{
    // Iterative version of the exponentiation by squaring algorithm.
    // https://en.wikipedia.org/wiki/Exponentiation_by_squaring
    uint32_t sq  = HASH_PRIME;
    uint32_t pow = 1U;

    for (; n > 0UL; n >>= 1UL) {
        // y * x^n:
        //  When n is even =>  y *     (x^2)^(n / 2)
        //  When n is odd  =>  y * x * (x^2)^((n - 1) / 2)
        //
        // Note that we don't need to do `n - 1` since the right-shift
        // already discards that bit.
        if ((n & 1UL) != 0UL) {
            pow = sq * pow;
        }
        sq = sq * sq;
    }
     return pow;
}

/// Description:
///     Finds the first occurrence of the pattern in the source text.
bool
string_find(
    char const *s,       size_t s_len,
    char const *pattern, size_t pattern_len,
    size_t *    out_i)
{
    // Edge cases and niche optimizations.
    switch (pattern_len) {
    // Conceptually, the empty string is present everywhere. So the first
    // occurrence is always at the very beginning.
    case 0:
        if (out_i) {
            *out_i = 0;
        }
        return true;

    // Patterns of length 1 can be treated as single-character lookups.
    case 1:
        return string_find_char(s, s_len, pattern[0], out_i);
    default:
        // The source text and pattern are the same length so all we
        // can do is to check for strict equality.
        if (s_len == pattern_len) {
            return string_eq_unsafe(s, pattern, pattern_len);
        }
        // The source text is smaller than the pattern, so the pattern
        // could not possibly be a substring anywhere in the source.
        else if (s_len < pattern_len) {
            return false;
        }
        break;
    }

    uint32_t const power  = rabin_karp_pow(pattern_len);
    uint32_t const p_hash = rabin_karp_hash(pattern, pattern_len);
    uint32_t       h      = rabin_karp_hash(s, pattern_len);

    // Substring is in the very first slot?
    // This is necessary so we can immediately hash the next string view
    // in the loop rather than defer it to the increment, because doing so
    // has clearer semantics on what values change.
    if (h == p_hash && string_eq_unsafe(s, pattern, pattern_len)) {
        if (out_i) {
            *out_i = 0;
        }
        return true;
    }

    size_t j = s_len - pattern_len + 1;
    for (size_t i = 1; i < j; i++) {
        char const *substr     = &s[i];
        uint32_t    prev_digit = cast(uint32_t)s[i - 1] * power;
        uint32_t    next_digit = cast(uint32_t)substr[pattern_len - 1];

        // Increase the entire hash by 1 place value before popping off
        // the first digit, then add on the next digit at the lowest
        // place value.
        h = (h * HASH_PRIME - prev_digit) + next_digit;
        if (h == p_hash && string_eq_unsafe(substr, pattern, pattern_len)) {
            if (out_i) {
                *out_i = i;
            }
            return true;
        }
    }
    return false;
}

/// Assumptions:
///     The given range is valid, i.e. it is in range of the given source
///     text's implied range (known to the caller, but not passed here).
char const *
string_slice_unsafe(char const *src, size_t i, size_t j, size_t *out_len)
{
    *out_len = j - i;
    return src + i;
}

bool
string_find_it(
    char const *s,       size_t  s_len,       // invariant
    char const *pattern, size_t  pattern_len, // invariant
    size_t *    out_i,   size_t *out_j)       // state
{
    // Current start index is always the previous stop.
    // On first iteration, we need to have initialized the
    // out-parameter to 0.
    size_t i = *out_j;

    // Local state needed for the string find.
    size_t      rest_len = 0;
    char const *rest     = string_slice_unsafe(s, i, s_len, &rest_len);
    size_t      rel_i    = 0;

    // The call is assumed to never dereference invalid memory.
    bool ok = string_find(rest, rest_len, pattern, pattern_len, &rel_i);
    if (ok) {
        i      = i + rel_i;
        *out_i = i;
        *out_j = i + pattern_len;
    }
    return ok;
}

static size_t
cstring_len(char const *s)
{
    size_t n = 0;
    while (s[n] != 0) {
        n++;
    }
    return n;
}

int
main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "[USAGE] %s <pattern> <strings...>\n", argv[0]);
        return 1;
    }

    char   const *const p     = argv[1];
    size_t const        p_len = cstring_len(p);
    for (int arg_i = 2; arg_i < argc; arg_i++) {
        char const *s     = argv[arg_i];
        size_t      s_len = cstring_len(s);
        printf("\"%s\"\n", s);

        for (size_t i = 0, j = 0; string_find_it(s, s_len, p, p_len, &i, &j);) {
            char const *substr = &s[i];
            printf(" + [%zu:%zu] = \"%.*s\"\n", i, j, cast(int)p_len, substr);
        }
    }
    return 0;
}

