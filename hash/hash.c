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

#define STRING_FMT_ARG(s)    cast(int)(s).len, (s).data

typedef struct String String;
struct String {
    char const *data;
    size_t      len;
};

typedef struct StringRange StringRange;
struct StringRange {
    size_t start;
    size_t end;
};

static String
string_make(char const *data, size_t len)
{
    String s = {data, len};
    return s;
}

/// Assumptions:
///     The given range is valid, i.e. it is in range of the given source
///     text's implied range (known to the caller, but not passed here).
static String
string_slice(String s, size_t i, size_t j)
{
    char const * data = s.data + i;
    size_t const len  = j - i;
    // DBG("text[%zu:%zu] = \"%.*s\"", i, j, cast(int)s_len, s);
    return string_make(data, len);
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
string_slice_from_inplace(String *s, size_t i)
{
    *s = string_slice(*s, i, s->len);
}

/// Assumptions:
///     Both of the given strings are already of the same length.
bool
string_eq_unsafe(String a, String b)
{
    for (size_t i = 0; i < a.len; i++) {
        if (a.data[i] != b.data[i]) {
            return false;
        }
    }
    return true;
}

bool
string_eq(String a, String b)
{
    return a.len == b.len && string_eq_unsafe(a, b);
}

bool
string_find_char(String s, char c, size_t *out_i)
{
    for (size_t i = 0; i < s.len; i++) {
        if (s.data[i] == c) {
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
rabin_karp_hash(String s)
{
    uint32_t hash  = 0U;
    for (size_t i = 0UL; i < s.len; i++) {
        // Multiplying the previous result by the base 'pushes up'
        // the previous digits by one place value. This is the same as
        // multiplying the current character by the largest unoccupied
        // place value and decrementing the exponent.
        hash = hash * HASH_PRIME + cast(uint32_t)s.data[i];
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
string_find(String text, String substr, size_t *out_i) {
    // Edge cases and niche optimizations.
    switch (substr.len) {
    // Conceptually, the empty string is present everywhere. So the first
    // occurrence is always at the very beginning.
    case 0:
        if (out_i) {
            *out_i = 0;
        }
        return true;

    // Patterns of length 1 can be treated as single-character lookups.
    case 1:
        return string_find_char(text, substr.data[0], out_i);
    default:
        // The source text and pattern are the same length so all we
        // can do is to check for strict equality.
        if (text.len == substr.len) {
            return string_eq_unsafe(text, substr);
        }
        // The source text is smaller than the pattern, so the pattern
        // could not possibly be a substring anywhere in the source.
        else if (text.len < substr.len) {
            return false;
        }
        break;
    }

    // Loop invariants.
    uint32_t const power       = rabin_karp_pow(substr.len);
    uint32_t const substr_hash = rabin_karp_hash(substr);

    // Loop state (mutable).
    String   curr_slice  = string_slice(text, 0, substr.len);
    uint32_t curr_hash   = rabin_karp_hash(curr_slice);

    // Substring is in the very first slot?
    // This is necessary so we can immediately hash the next string view
    // in the loop rather than defer it to the increment, because doing so
    // has clearer semantics on what values change.
    if (curr_hash == substr_hash && string_eq_unsafe(text, substr)) {
        if (out_i) {
            *out_i = 0;
        }
        return true;
    }

    // 1 past the last index that we could possibly have a window of the
    // substring.
    size_t const j = text.len - substr.len + 1;
    for (size_t i = 1; i < j; i++) {
        curr_slice = string_slice(text, i, i + substr.len);

        uint32_t prev_digit = cast(uint32_t)text.data[i - 1] * power;
        uint32_t next_digit = cast(uint32_t)curr_slice.data[substr.len - 1];

        // Increase the entire hash by 1 place value before popping off
        // the first digit, then add on the next digit at the lowest
        // place value.
        curr_hash = (curr_hash * HASH_PRIME - prev_digit) + next_digit;
        if (curr_hash == substr_hash && string_eq_unsafe(curr_slice, substr)) {
            if (out_i) {
                *out_i = i;
            }
            return true;
        }
    }
    return false;
}

bool
string_find_it(String text, String substr, StringRange *out)
{
    // Current start index is always the previous stop.
    // On first iteration, we need to have initialized the
    // out-parameter to 0.
    size_t const i = out->end;

    // Local state needed for the string find.
    String rest = string_slice(text, i, text.len);

    // The string find call is assumed to never dereference invalid memory.
    size_t rel_i;
    if (!string_find(rest, substr, &rel_i)) {
        return false;
    }

    out->start = i + rel_i;
    out->end   = out->start + substr.len;
    return true;
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
string_match_opt(String text, String regex, StringRange *out);

static bool
string_match_star(String text, String regex, StringRange *out);

/// Description:
///     Matches the given regular expression at the start of
///     the source text.
///
/// Returns:
///     `true` if successfully matched. In this case, the starting index is
///     assumed to be 0 and the ending index is stored in the out parameter.
///     
static bool
string_match_begin(String text, String regex, StringRange *out)
{
    // Matching the empty string is always a success.
    if (regex.len == 0) {
        DBG("...matched empty [regex=\"\"] on [text[:%zu]=\"%.*s\"]!",
                text.len, STRING_FMT_ARG(text));
        out->end = 0;
        return true;
    }

    // We know by now that the regex has at least one character, so this is
    // guaranteed to be safe.
    char const regex0 = regex.data[0];

    // Matching the end of the string?
    if (regex.len == 1 && regex0 == '$' && text.len == 0) {
        DBG("...matched end [regex=\"$\"] on [text[:%zu]=\"%.*s\"]!",
                text.len, STRING_FMT_ARG(text));
        out->end = 0;
        return true;
    }

    // Matching some optional or a zero-or-more character?
    // The character to be matched is at regex[0].
    if (regex.len > 1) switch (regex.data[1]) {
    case '?': return string_match_opt(text, regex, out);
    case '*': return string_match_star(text, regex, out);
    default:  break;
    }

    // An empty source text always compares false with any non-empty regex.
    if (text.len == 0) {
        return false;
    }

    // By this point we know that both the text and regex have lengths
    // greater than 0.
    if (!string_match_one(text.data[0], regex0)) {
        return false;
    }

    DBG("...matched [regex=\"%c\"] at [text[:%zu]=\"%.*s\"]!",
            regex0, text.len, STRING_FMT_ARG(text));

    // Prepare recursive call
    out->end = 1;
    string_slice_from_inplace(&text,  1);
    string_slice_from_inplace(&regex, 1);
    
    // Successful, non-empty recursive calls will propagate a steady stream
    // of 1's which we'll accumulate.
    StringRange rel;
    if (!string_match_begin(text, regex, &rel)) {
        return false;
    }

    DBG("[rel_j = %zu] [*out_j = %zu + %zu]", rel.end, out->end, rel.end);
    out->end += rel.end;
    return true;
}

/// Assumptions:
///     The optional character to be matched is at `regex[0]` and
///     the question mark character is at `regex[1]`.
static bool
string_match_opt(String text, String regex, StringRange *out)
{
    char option = regex.data[0];
    // Found the character in question?
    if (string_match_one(text.data[0], option)) {
        DBG("...matched optional [regex=\"%c\"] at [text[0:]=\"%.*s\"]!",
                option, STRING_FMT_ARG(text));

        string_slice_from_inplace(&text, 1);
        out->end = 1;
    } else {
        out->end = 0;
    }
    string_slice_from_inplace(&regex, 2);
    DBG("...recursively matching [regex=\"%.*s\"] at [text=\"%.*s\"]...",
            STRING_FMT_ARG(regex), STRING_FMT_ARG(text));

    StringRange rel;
    if (!string_match_begin(text, regex, &rel)) {
        return false;
    }

    DBG("[rel_j = %zu] [*out_j = %zu + %zu]", rel.end, out->end, rel.end);
    out->end += rel.end;
    return true;
}

/// Assumptions:
///     The character to be matched is at `regex[0]`
///     and the star character itself is at `regex[1]`.
static bool
string_match_star(String text, String regex, StringRange *out)
{
    char option = regex.data[0];
    if (string_match_one(text.data[0], option)) {
        DBG("...matched star [regex=\"%c\"] at [text[0:]=\"%.*s\"]!",
                option, STRING_FMT_ARG(text));

        // Don't slice the pattern- we want to re-match the star on
        // recursive calls.
        string_slice_from_inplace(&text, 1);
        out->end = 1;
    } else {
        // TODO(2026-10-13):
        //  Fix edge case when no match. E.g. "h*" in "hihhihhhi"
        DBG("...no matched star [regex=\"%c\"] at [text[0:]=\"%.*s\"]!",
                option, STRING_FMT_ARG(text));
        string_slice_from_inplace(&regex, 2);
        out->end = 0;
    }
    DBG("...recursively matching [regex=\"%.*s\"] at [text=\"%.*s\"]...",
            STRING_FMT_ARG(regex), STRING_FMT_ARG(text));

    StringRange rel;
    if (!string_match_begin(text, regex, &rel)) {
        return false;
    }

    DBG("[rel_j = %zu] [*out_j = %zu + %zu]", rel.end, out->end, rel.end);
    out->end += rel.end;
    return true;
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
string_match(String text, String regex, StringRange *out)
{
    out->start = 0;
    out->end   = 0;

    // We only need to try to match at the beginning. No need to iterate
    // through all possible positions of the text.
    if (regex.len > 0 && regex.data[0] == '^') {
        string_slice_from_inplace(&regex, 1);
        return string_match_begin(text, regex, out);
    }

    // Try to find the first occurrence of the given regular expression
    // (including the empty string) anywhere in the source text. 
    for (size_t text_i = 0; text_i < text.len; text_i += 1) {
        String rest = string_slice(text, text_i, text.len);

        // Return the first match to the caller.
        StringRange rel;
        if (string_match_begin(rest, regex, &rel)) {
            out->start = text_i;
            out->end   = text_i + rel.end;
            return true;
        }
    }
    return false;
}

bool
string_match_it(String text, String regex, StringRange *out)
{
    // Current start index is always the previous stop.
    // On first iteration, we need to have initialized the
    // out-parameter to 0.
    size_t const curr_i = out->end;

    // Local state needed for the string find.
    String rest = string_slice(text, curr_i, text.len);
    DBG("Matching [regex=\"%.*s\"] at [text[%zu:]=\"%.*s\"]...",
            STRING_FMT_ARG(regex), curr_i, STRING_FMT_ARG(rest));

    // The string find call is assumed to never dereference invalid memory.
    StringRange rel;
    if (!string_match(rest, regex, &rel)) {
        return false;
    }

    out->start = curr_i + rel.start;
    out->end   = curr_i + rel.end;
    DBG("...match found at text[%zu:%zu]!", out->start, out->end);
    return true;
}

static size_t
cstring_len(char const *cstring)
{
    size_t len = 0;
    while (cstring[len] != 0) {
        len++;
    }
    return len;
}

static String
string_from_cstring(char const *cstring)
{
    size_t len = cstring_len(cstring);
    return string_make(cstring, len);
}

int
main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "[USAGE] %s <pattern> <strings...>\n", argv[0]);
        return 1;
    }

    String const pattern = string_from_cstring(argv[1]);
    for (int argv_i = 2; argv_i < argc; argv_i++) {
        String text = string_from_cstring(argv[argv_i]);
        printf("\"%.*s\"\n", STRING_FMT_ARG(text));

        for (StringRange range = {0, 0};
                string_match_it(text, pattern, &range);
                /* empty */)
        {
            String match = string_slice(text, range.start, range.end);
            printf(" + [%zu:%zu] = \"%.*s\"\n",
                    range.start, range.end, STRING_FMT_ARG(match));
        }
        printf(" - <no more matches>\n");
    }
    return 0;
}

#undef STRING_FMT_ARG
