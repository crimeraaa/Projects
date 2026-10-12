#include <stdbool.h> // bool, true, false
#include <stddef.h>  // size_t
#include <stdint.h>  // uint64_t
#include <stdio.h>   // fprintf, printf

#define cast(T) (T)

#define DBG(fmt, ...) \
    do { \
        fprintf(stderr, __FILE__ ":%-3i in %-18s : " fmt "\n", \
                __LINE__, __func__, __VA_ARGS__); \
    } while (false)

/// Assumptions:
///     The given range is valid, i.e. it is in range of the given source
///     text's implied range (known to the caller, but not passed here).
static char const *
string_slice_unsafe(char const *text, size_t i, size_t j, size_t *out_len)
{
    char const * s     = text + i;
    size_t const s_len = j - i;
    // DBG("text[%zu:%zu] = \"%.*s\"", i, j, cast(int)s_len, s);
    *out_len = s_len;
    return s;
}

/// Description:
///     Slices the given string using the half-open range `[i,*len)`.
///
/// Assumptions:
///     The given range is valid.
///
/// Returns:
///     The resulting slice is stored in in-out-parameters.
static void
string_slice_from_inplace(char const **text, size_t *text_len, size_t i)
{
    *text = string_slice_unsafe(*text, i, *text_len, text_len);
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

#define HASH_PRIME  16777619U

static uint32_t
rabin_karp_hash(char const *s, size_t n)
{
    uint32_t hash  = 0U;
    for (size_t i = 0UL; i < n; i++) {
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
///     Finds the first occurrence of the substring in the source text.
bool
string_find(
        char const *text,   size_t text_len,
        char const *substr, size_t susbtr_len,
        size_t *    out_i
) {
    // Edge cases and niche optimizations.
    switch (susbtr_len) {
    // Conceptually, the empty string is present everywhere. So the first
    // occurrence is always at the very beginning.
    case 0:
        if (out_i) {
            *out_i = 0;
        }
        return true;

    // Patterns of length 1 can be treated as single-character lookups.
    case 1:
        return string_find_char(text, text_len, substr[0], out_i);
    default:
        // The source text and pattern are the same length so all we
        // can do is to check for strict equality.
        if (text_len == susbtr_len) {
            return string_eq_unsafe(text, substr, susbtr_len);
        }
        // The source text is smaller than the pattern, so the pattern
        // could not possibly be a substring anywhere in the source.
        else if (text_len < susbtr_len) {
            return false;
        }
        break;
    }

    uint32_t const power       = rabin_karp_pow(susbtr_len);
    uint32_t const substr_hash = rabin_karp_hash(substr, susbtr_len);
    uint32_t       curr_hash   = rabin_karp_hash(text,   susbtr_len);

    // Substring is in the very first slot?
    // This is necessary so we can immediately hash the next string view
    // in the loop rather than defer it to the increment, because doing so
    // has clearer semantics on what values change.
    if (curr_hash == substr_hash
            && string_eq_unsafe(text, substr, susbtr_len))
    {
        if (out_i) {
            *out_i = 0;
        }
        return true;
    }

    size_t j = text_len - susbtr_len + 1;
    for (size_t i = 1; i < j; i++) {
        char const *text_slice = &text[i];
        uint32_t    prev_digit = cast(uint32_t)text[i - 1] * power;
        uint32_t    next_digit = cast(uint32_t)text_slice[susbtr_len - 1];

        // Increase the entire hash by 1 place value before popping off
        // the first digit, then add on the next digit at the lowest
        // place value.
        curr_hash = (curr_hash * HASH_PRIME - prev_digit) + next_digit;
        if (curr_hash == substr_hash
                && string_eq_unsafe(text_slice, substr, susbtr_len))
        {
            if (out_i) {
                *out_i = i;
            }
            return true;
        }
    }
    return false;
}

bool
string_find_it(
        char const *     text,   size_t           text_len,
        char const *     substr, size_t           substr_len,
        size_t *restrict out_i,  size_t *restrict out_j
) {
    // Current start index is always the previous stop.
    // On first iteration, we need to have initialized the
    // out-parameter to 0.
    size_t const i = *out_j;

    // Local state needed for the string find.
    size_t      rest_len;
    char const *rest = string_slice_unsafe(text, i, text_len, &rest_len);

    // The string find call is assumed to never dereference invalid memory.
    size_t rel_i;
    bool   ok = string_find(rest, rest_len, substr, substr_len, &rel_i);
    if (ok) {
        *out_i = i + rel_i;
        *out_j = *out_i + substr_len;
    }
    return ok;
}

static bool
string_match_one(char text, char regex)
{
    // Any inputted text matches the wildcard.
    if (regex == '.') {
        return true;
    }
    return text == regex;
}

static bool
string_match_opt(
        char const *text,   size_t  text_len,
        char const *regex,  size_t  regex_len,
        char        option, size_t *out_j);

static bool
string_match_star(
        char const *text,   size_t  text_len,
        char const *regex,  size_t  regex_len,
        char        option, size_t *out_j);

/// Description:
///     Matches the given regular expression at the start of
///     the source text.
///
/// Returns:
///     `true` if successfully matched. In this case, the starting index is
///     assumed to be 0 and the ending index is stored in the out parameter.
///     
static bool
string_match_begin(
        char const *text,  size_t text_len,
        char const *regex, size_t regex_len,
        size_t *    out_j
) {
    // Matching the empty string is always a success.
    if (regex_len == 0) {
        DBG("...matched empty [regex=\"\"] on [text[:%zu]=\"%.*s\"]!",
                text_len,
                cast(int)text_len, text);
        *out_j = 0;
        return true;
    }

    // Matching the end of the string?
    if (regex_len == 1 && regex[0] == '$' && text_len == 0) {
        DBG("...matched end [regex=\"$\"] on [text[:%zu]=\"%.*s\"]!",
                text_len,
                cast(int)text_len, text);
        *out_j = 0;
        return true;
    }

    // Matching some optional or a zero-or-more character?
    // The character to be matched is at regex[0].
    if (regex_len > 1) switch (regex[1]) {
    case '?':
        return string_match_opt(
                text,     text_len,
                regex,    regex_len,
                regex[0], out_j);
    case '*':
        return string_match_star(
                text,     text_len,
                regex,    regex_len,
                regex[0], out_j);
    default:
        break;
    }

    // An empty source text always compares false with any non-empty regex.
    if (text_len == 0) {
        return false;
    }

    // By this point we know that both the text and regex have lengths
    // greater than 0.
    if (!string_match_one(text[0], regex[0])) {
        return false;
    }

    DBG("...matched [regex=\"%c\"] at [text[:%zu]=\"%.*s\"]!",
            regex[0],
            text_len,
            cast(int)text_len, text);

    // Prepare recursive call
    *out_j = 1;
    string_slice_from_inplace(&text,  &text_len,  1);
    string_slice_from_inplace(&regex, &regex_len, 1);
    
    size_t rel_j;
    bool   ok = string_match_begin(
            text,  text_len,
            regex, regex_len,
            &rel_j);

    // Successful, non-empty recursive calls will propagate a steady stream
    // of 1's which we'll accumulate.
    if (ok) {
        DBG("[rel_j = %zu] [*out_j = %zu + %zu]", rel_j, *out_j, rel_j);
        *out_j += rel_j;
    }
    return ok;
}

/// Assumptions:
///     The optional character to be matched is at `regex[0]` and
///     the question mark character is at `regex[1]`.
static bool
string_match_opt(
        char const *text,   size_t  text_len,
        char const *regex,  size_t  regex_len,
        char        option, size_t *out_j
) {
    // Found the character in question?
    if (string_match_one(text[0], option)) {
        DBG("...matched optional [regex=\"%c\"] at [text[0:]=\"%.*s\"]!",
                option,
                cast(int)text_len, text);

        string_slice_from_inplace(&text,  &text_len, 1);
        *out_j = 1;
    } else {
        *out_j = 0;
    }
    string_slice_from_inplace(&regex, &regex_len, 2);
    DBG("...recursively matching [regex=\"%.*s\"] at [text=\"%.*s\"]...",
            cast(int)regex_len, regex,
            cast(int)text_len,  text);

    size_t rel_j;
    bool   ok = string_match_begin(
            text,  text_len,
            regex, regex_len,
            &rel_j);

    if (ok) {
        DBG("[rel_j = %zu] [*out_j = %zu + %zu]", rel_j, *out_j, rel_j);
        *out_j += rel_j;
    }
    return ok;
}

/// Assumptions:
///     The character to be matched is at `regex[0]`
///     and the star character itself is at `regex[1]`.
static bool
string_match_star(
        char const *text,   size_t  text_len,
        char const *regex,  size_t  regex_len,
        char        option, size_t *out_j
) {
    if (string_match_one(text[0], option)) {
        DBG("...matched star [regex=\"%c\"] at [text[0:]=\"%.*s\"]!",
                option,
                cast(int)text_len, text);

        // Don't slice the pattern- we want to re-match the star on
        // recursive calls.
        string_slice_from_inplace(&text, &text_len, 1);
        *out_j = 1;
    } else {
        // TODO(2026-10-13):
        //  Fix edge case when no match. E.g. "h*" in "hihhihhhi"
        DBG("...no matched star [regex=\"%c\"] at [text[0:]=\"%.*s\"]!",
                option,
                cast(int)text_len, text);
        string_slice_from_inplace(&regex, &regex_len, 2);
        *out_j = 0;
    }
    DBG("...recursively matching [regex=\"%.*s\"] at [text=\"%.*s\"]...",
            cast(int)regex_len, regex,
            cast(int)text_len,  text);

    size_t rel_j;
    bool   ok = string_match_begin(
            text,  text_len,
            regex, regex_len,
            &rel_j);

    if (ok) {
        DBG("[rel_j = %zu] [*out_j = %zu + %zu]", rel_j, *out_j, rel_j);
        *out_j += rel_j;
    }
    return ok;
}

/// Description:
///     Finds the first occurence of the given regular expression anywhere
///     in the source text.
///
/// Returns:
///     `true` if we found a match, else `false`.
///     In the `true` case, the half-open index range is stored in the out
///     parameters.
bool
string_match(
        char const *      text,  size_t           text_len,
        char const *      regex, size_t           regex_len,
        size_t *restrict  out_i, size_t *restrict out_j
) {
    *out_i = 0;
    *out_j = 0;
    if (regex_len > 0 && regex[0] == '^') {
        string_slice_from_inplace(&regex, &regex_len, 1);
        return string_match_begin(
                text,  text_len,
                regex, regex_len,
                out_j);
    } else {
        // Try to find the first occurrence of the given regular expression
        // (including the empty string) anywhere in the source text. 
        for (size_t text_i = 0; text_i < text_len; text_i += 1) {
            size_t      rest_len;
            char const *rest = string_slice_unsafe(
                    text, text_i, text_len,
                    &rest_len);

            size_t rel_j;
            bool   ok = string_match_begin(
                    rest,   rest_len,
                    regex,  regex_len,
                    &rel_j);

            // Return the first match to the caller.
            if (ok) {
                *out_i = text_i;
                *out_j = text_i + rel_j;
                return true;
            }
        }
    }
    return false;
}

bool
string_match_it(
        char const *     text,  size_t           text_len,
        char const *     regex, size_t           regex_len,
        size_t *restrict out_i, size_t *restrict out_j
) {
    // Current start index is always the previous stop.
    // On first iteration, we need to have initialized the
    // out-parameter to 0.
    size_t const i = *out_j;

    // Local state needed for the string find.
    size_t      rest_len;
    char const *rest = string_slice_unsafe(text, i, text_len, &rest_len);

    // Assume both are nul-terminated for now.
    DBG("Matching [regex=\"%s\"] at [text[%zu:]=\"%s\"]...", regex, i, rest);

    // The string find call is assumed to never dereference invalid memory.
    size_t rel_i, rel_j;
    bool   ok = string_match(
            rest, rest_len,
            regex, regex_len,
            &rel_i, &rel_j);
    if (ok) {
        *out_i = i + rel_i;
        *out_j = i + rel_j;
        DBG("...match found at text[%zu:%zu]!", *out_i, *out_j);
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

static char const *
string_from_cstring(char const *cs, size_t *out_len)
{
    *out_len = cstring_len(cs);
    return cs;
}

int
main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "[USAGE] %s <pattern> <strings...>\n", argv[0]);
        return 1;
    }

    size_t      pattern_len;
    char const *pattern = string_from_cstring(argv[1], &pattern_len);
    for (int argv_i = 2; argv_i < argc; argv_i++) {
        size_t      text_len;
        char const *text = string_from_cstring(argv[argv_i], &text_len);
        printf("\"%s\"\n", text);

        for (size_t i = 0, j = 0;
            string_match_it(
                text,    text_len,
                pattern, pattern_len,
                &i, &j);
            /* empty */)
        {
            char const *text_slice = &text[i];
            printf(" + [%zu:%zu] = \"%.*s\"\n",
                    i, j,
                    cast(int)(j - i), text_slice);
        }
        printf(" - <no more matches>\n");
    }
    return 0;
}

