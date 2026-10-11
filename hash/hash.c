#include <stdbool.h> // bool, true, false
#include <stddef.h>  // size_t
#include <stdint.h>  // uint64_t
#include <stdio.h>   // fprintf, printf

#define cast(T) (T)

bool
string_find_char(char const *s, size_t n, char c, size_t *out)
{
    for (size_t i = 0; i < n; i++) {
        if (s[i] == c) {
            if (out) {
                *out = i;
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

static bool
size_overflow_sub(size_t *out, size_t a, size_t b)
{
    // a - b >= 0
    // a + (-b) >= 0
    // a + (-b) - (-b) >= 0 - (-b)
    if (a >= b) {
        *out = a - b;
        return false;
    } else {
        return true;
    }
}

#define HASH_PRIME  16777619

static uint32_t
hash_rabin_karp(char const *s, size_t n)
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
pow_rabin_karp(size_t n)
{
    uint32_t pow = 1;
    while (n-- > 0) {
        pow *= HASH_PRIME;
    }
    return pow;
}

/// Description:
///     Finds the first occurrence of the substring `p` in the source text `s`.
bool
string_find_string(char const *s, size_t s_len, char const *p, size_t p_len, size_t *out)
{
    // Niche optimizations.
    switch (p_len) {
    case 0:
        if (out) {
            *out = 0;
        }
        return true;
    case 1:  return string_find_char(s, s_len, p[0], out);
    default: break;
    }

    size_t j;
    if (size_overflow_sub(&j, s_len, p_len)) {
        return false;
    }

    uint32_t const power  = pow_rabin_karp(p_len);
    uint32_t const p_hash = hash_rabin_karp(p, p_len);
    uint32_t       s_hash = hash_rabin_karp(s, p_len);

    // Substring is in the very first slot?
    // This is necessary so we can immediately hash the next string view
    // in the loop rather than defer it to the increment, because doing so
    // has clearer semantics on what values change.
    if (s_hash == p_hash && string_eq_unsafe(s, p, p_len)) {
        if (out) {
            *out = 0;
        }
        return true;
    }

    for (size_t i = 1; i <= j; i++) {
        char const *substr     = &s[i];
        uint32_t    prev_digit = cast(uint32_t)s[i - 1] * power;
        uint32_t    next_digit = cast(uint32_t)substr[p_len - 1];

        // Increase the entire hash by 1 place value before popping off
        // the first digit, then add on the next digit at the lowest
        // place value.
        s_hash = (s_hash * HASH_PRIME - prev_digit) + next_digit;
        if (s_hash == p_hash && string_eq_unsafe(substr, p, p_len)) {
            if (out) {
                *out = i;
            }
            return true;
        }
    }
    return false;
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
        char   const *s     = argv[arg_i];
        size_t        s_len = cstring_len(s);
        printf("\"%s\"\n", s);
        for (size_t i = 0, j = 0; /* empty */; i = j) {
            // Since we get the index relative to a substring, don't
            // immediately overwrite `i`. We'll use this to calculate the
            // actual abolute index.
            size_t start;
            if (!string_find_string(&s[i], s_len - i, p, p_len, &start)) {
                break;
            }

            i = i + start;
            j = i + p_len;
            printf("+ [%zu:%zu] = \"%.*s\"\n", i, j, cast(int)p_len, &s[i]);
        }
    }
    return 0;
}

