#ifndef STRINGS_BUILDER_H
#define STRINGS_BUILDER_H

#include "../mem/mem.h"
#include "../strings/strings.h"

#ifndef STRINGS_BUILDER_DEF
#define STRINGS_BUILDER_DEF
#endif // !STRINGS_BUILDER_DEF

typedef struct string_Builder string_Builder;
struct string_Builder {
    // A 1-dimensonal array allocated by `allocator`.
    char *data;

    // The number of actively used characters in `data`.
    size_t len;

    // The number of allocated characters for `data`.
    size_t cap;

    // We track our allocator for convenience.
    mem_Allocator allocator;
};

STRINGS_BUILDER_DEF void
string_builder_init_none(string_Builder *b, mem_Allocator allocator);

STRINGS_BUILDER_DEF mem_Allocator_Error
string_builder_init_cap(string_Builder *b, size_t n, mem_Allocator allocator);

STRINGS_BUILDER_DEF void
string_builder_reset(string_Builder *b);

STRINGS_BUILDER_DEF mem_Allocator_Error
string_write_char(string_Builder *b, char c);

STRINGS_BUILDER_DEF mem_Allocator_Error
string_write_string(string_Builder *b, string_View s);

STRINGS_BUILDER_DEF mem_Allocator_Error
string_write_cstring(string_Builder *b, const char *s);

#define string_write_literal(b, s) \
    string_write_string(b, string_make(s, sizeof(s) - 1))

STRINGS_BUILDER_DEF mem_Allocator_Error
string_write_uint(string_Builder *b, uint u, int base);

STRINGS_BUILDER_DEF mem_Allocator_Error
string_write_int(string_Builder *b, int i, int base);

STRINGS_BUILDER_DEF char
string_pop_char(string_Builder *b);

STRINGS_BUILDER_DEF string_View
string_to_string(const string_Builder *b);

STRINGS_BUILDER_DEF const char *
string_to_cstring(const string_Builder *b);

STRINGS_BUILDER_DEF void
string_builder_destroy(string_Builder *b);

#ifdef STRINGS_BUILDER_IMPLEMENTATION

#include <limits.h>

#ifdef _MSC_VER
#include <intrin.h> // _BitScanReverse
#endif

STRINGS_BUILDER_DEF void
string_builder_init_none(string_Builder *b, mem_Allocator allocator)
{
    b->data      = NULL;
    b->len       = 0;
    b->cap       = 0;
    b->allocator = allocator;
}

STRINGS_BUILDER_DEF mem_Allocator_Error
string_builder_init_cap(string_Builder *b, size_t n, mem_Allocator allocator)
{
    mem_Allocator_Error err = MEM_OK;
    b->data = mem_alloc_array(char, allocator, n, &err);
    b->len  = 0;
    b->cap  = (b->data != NULL) ? n : 0;
    return err;
}

STRINGS_BUILDER_DEF void
string_builder_reset(string_Builder *b)
{
    b->len = 0;
}

STRINGS_BUILDER_DEF mem_Allocator_Error
string_write_char(string_Builder *b, char c)
{
    mem_Allocator_Error err = MEM_OK;
    size_t cap = b->cap;
    if (b->len + 2 > cap) {
        char *new_data;
        size_t new_cap;

        new_cap = (cap > 8) ? cap * 2 : 8;
        new_data = mem_resize_array(char, b->allocator, b->data, cap, new_cap, &err);
        if (new_data == NULL) {
            return err;
        }
        b->data = new_data;
        b->cap  = new_cap;
    }

    b->data[b->len++] = c;
    b->data[b->len]   = 0;
    return err;
}

STRINGS_BUILDER_DEF mem_Allocator_Error
string_write_string(string_Builder *b, string_View s)
{
    mem_Allocator_Error err = MEM_OK;
    size_t new_len;

    new_len = b->len + s.len;
    // String length plus nul would overflow?
    if (new_len + 1 > b->cap) {
        char *new_data;
        size_t new_cap;

        new_cap = b->cap * 2;
        // String length plus nul is larger than even our growth?
        if (new_len + 1 > new_cap) {
            new_cap = new_len + 1;
        }

        // If not a power of 2, clamp it to the next one.
        if ((new_cap & (new_cap - 1)) == 0) {
            size_t tmp = 1;
            while (tmp < new_cap) {
                tmp *= 2;
            }
            new_cap = tmp;
        }

        new_data = mem_resize_array(char, b->allocator, b->data, b->cap, new_cap, &err);
        if (new_data == NULL) {
            return err;
        }

        b->data = new_data;
        b->cap  = new_cap;
    }

    memcpy(&b->data[b->len], s.data, s.len);
    // We allocated at least len + 1, so this is a valid index.
    b->data[new_len] = 0;
    b->len = new_len;
    return err;
}

STRINGS_BUILDER_DEF mem_Allocator_Error
string_write_cstring(string_Builder *b, const char *s)
{
    string_View sv = string_make_cstring(s);
    return string_write_string(b, sv);
}

STRINGS_BUILDER_DEF mem_Allocator_Error
string_write_uint(string_Builder *b, uint u, int base)
{
    local_persist const char DIGIT_CHARS[] = "01234567890ABDEFGHIJKLMNOPQRSTUVWXYZ";

    // Reasonable to assume that maximum binary length more than sufficient
    // for all other base representations.
    char buf[bit_size(u) + 2];
    char *end, *p;

    // We don't nul-terminate the buffer since we track the length anyyay.
    end  = buf + sizeof(buf);
    p    = end - 1;
    if (u == 0) {
        // `p` now points to 1 before the LSD.
        *p-- = '0';
    }
    // Write LSD to MSD but in reverse of the buffer so we get the correct
    // order of digits.
    else {
        uint ubase = cast(uint)base;
        //
        // Base is a power of 2?
        if ((ubase & (ubase - 1)) == 0) {
            uint rshift = 0;

#ifdef __GNUC__
            rshift = __builtin_ctz(u);
#elif defined(_MSC_VER)
            // Windows guarantees that `int` and `long` are the same size
            // for historical reasons. So this is safe although UB.
            //
            // NOTE: Recheck this if ever MS changes their architecture!
            _BitScanReverse(cast(ulong *)&rshift, ubase);
#else // !__GNUC__ && !_MSC_VER
            // This works but we'd much prefer a builtin/intrinsic.
            for (uint tmp = u; (tmp & 1) == 0; tmp >>= 1) {
                rshift++;
            }
#endif // __GNUC__, _MSC_VER
            // Power of 2 arithmetic is faster than division/modulo.
            while (u > 0) {
                *p-- = DIGIT_CHARS[u & (ubase - 1)];
                u   >>= rshift;
            }
        }
        // Base isn't a power of 2, use division/modulo. This always
        // works but may be a lot slower.
        else {
            while (u > 0) {
                *p-- = DIGIT_CHARS[u % ubase];
                u   /= ubase;
            }
        }
    }

    // Correct `p` so that we point to the MSD, not 1 before it.
    p++;
    return string_write_string(b, string_make(p, cast(size_t)(end - p)));
}

STRINGS_BUILDER_DEF mem_Allocator_Error
string_write_int(string_Builder *b, int i, int base)
{
    // Get the absolute value of `i`. Use unsigned so we can represent
    // the absolute value of `INT_MIN` with a bit of work.
    uint u = cast(uint)i;
    if (i < 0) {
        mem_Allocator_Error err = string_write_char(b, '-');
        if (err) {
            return err;
        }
        u = 0 - u;
    }
    return string_write_uint(b, u, base);
}

STRINGS_BUILDER_DEF char
string_pop_char(string_Builder *b)
{
    size_t n, i;
    char c;

    // Nothing to do?
    n = b->len;
    if (n == 0) {
        return '\0';
    }

    // Read last valid character.
    i = n - 1;
    c = b->data[i];

    // Update, ensuring nul-termiantion.
    b->data[i] = '\0';
    b->len     = i;
    return c;
}

STRINGS_BUILDER_DEF string_View
string_to_string(const string_Builder *b)
{
    return string_make(b->data, b->len);
}

STRINGS_BUILDER_DEF const char *
string_to_cstring(const string_Builder *b)
{
    return b->data;
}

STRINGS_BUILDER_DEF void
string_builder_destroy(string_Builder *b)
{
    mem_free_array(b->allocator, b->data, b->cap, NULL);
    b->data = NULL;
    b->len  = 0;
    b->cap  = 0;
}

#endif // STRINGS_BUILDER_IMPLEMENTATION

#endif // !STRINGS_BUILDER_H
