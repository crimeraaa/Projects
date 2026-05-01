// standard
#include <string.h> // memset

// local
#include "ascii.h"

enum ascii_Trait {
    // Primary traits
    ASCII_CONTROL     = 1 << 0, // bit 0 is 1 if control else 0 if printable
    ASCII_SPACE       = 1 << 1,
    ASCII_BLANK       = 1 << 2,
    ASCII_PUNCT       = 1 << 3,
    ASCII_LOWER       = 1 << 4,
    ASCII_UPPER       = 1 << 5,
    ASCII_DECIMAL     = 1 << 6,
    ASCII_HEXADECIMAL = 1 << 7,

    // Derived traits
    ASCII_LETTER  = ASCII_LOWER  | ASCII_UPPER,
    ASCII_ALNUM   = ASCII_LETTER | ASCII_DECIMAL,
    ASCII_GRAPH   = ASCII_PUNCT  | ASCII_ALNUM,
};

struct ascii_Trait_Set {
    uint8_t traits;
};

// static const char *
// ascii_trait_string(enum ascii_Trait t)
// {
//     switch (t) {
//     // Primary traits
//     case ASCII_CONTROL:     return "control";
//     case ASCII_SPACE:       return "space";
//     case ASCII_BLANK:       return "blank";
//     case ASCII_PUNCT:       return "punct";
//     case ASCII_LOWER:       return "lower";
//     case ASCII_UPPER:       return "upper";
//     case ASCII_DECIMAL:     return "decimal";
//     case ASCII_HEXADECIMAL: return "hexadecimal";
//
//     // Derived traits
//     case ASCII_LETTER:  return "letter";
//     case ASCII_ALNUM:   return "alnum";
//     case ASCII_GRAPH:   return "graph";
//     }
//     return NULL;
// }

static const struct ascii_Trait_Set
ASCII_TRAITS[] = {
    // Control characters: [0,31)
    /* NUL: \0  */   {ASCII_CONTROL},
    /* SOH: \1  */   {ASCII_CONTROL},
    /* STX: \2  */   {ASCII_CONTROL},
    /* ETX: \3  */   {ASCII_CONTROL},
    /* EOT: \4  */   {ASCII_CONTROL},
    /* ENQ: \5  */   {ASCII_CONTROL},
    /* ACK: \6  */   {ASCII_CONTROL},
    /* BEL: \a  */   {ASCII_CONTROL},
    /* BS:  \b  */   {ASCII_CONTROL},
    /* HT:  \t  */   {ASCII_CONTROL | ASCII_SPACE | ASCII_BLANK},
    /* LF:  \n  */   {ASCII_CONTROL | ASCII_SPACE},
    /* VT:  \v  */   {ASCII_CONTROL | ASCII_SPACE},
    /* FF:  \f  */   {ASCII_CONTROL | ASCII_SPACE},
    /* CR:  \r  */   {ASCII_CONTROL | ASCII_SPACE},
    /* SO:  \16 */   {ASCII_CONTROL},
    /* SI:  \17 */   {ASCII_CONTROL},
    /* DLE: \20 */   {ASCII_CONTROL},
    /* DC1: \21 */   {ASCII_CONTROL},
    /* DC2: \23 */   {ASCII_CONTROL},
    /* DC3: \24 */   {ASCII_CONTROL},
    /* DC4: \25 */   {ASCII_CONTROL},
    /* NAK: \26 */   {ASCII_CONTROL},
    /* SYN: \27 */   {ASCII_CONTROL},
    /* ETB: \30 */   {ASCII_CONTROL},
    /* CAN: \31 */   {ASCII_CONTROL},
    /* EM:  \32 */   {ASCII_CONTROL},
    /* SUB: \33 */   {ASCII_CONTROL},
    /* ESC: \34 */   {ASCII_CONTROL},
    /* FS:  \35 */   {ASCII_CONTROL},
    /* GS:  \36 */   {ASCII_CONTROL},
    /* RS:  \37 */   {ASCII_CONTROL},
    /* US:  \40 */   {ASCII_CONTROL},

    // Printable characters: [32,128)
    /* ' ' */ {ASCII_SPACE | ASCII_BLANK},
    /*  !  */ {ASCII_PUNCT},
    /*  "  */ {ASCII_PUNCT},
    /*  #  */ {ASCII_PUNCT},
    /*  $  */ {ASCII_PUNCT},
    /*  %  */ {ASCII_PUNCT},
    /*  &  */ {ASCII_PUNCT},
    /*  '  */ {ASCII_PUNCT},
    /*  (  */ {ASCII_PUNCT},
    /*  )  */ {ASCII_PUNCT},
    /*  *  */ {ASCII_PUNCT},
    /*  +  */ {ASCII_PUNCT},
    /*  ,  */ {ASCII_PUNCT},
    /*  -  */ {ASCII_PUNCT},
    /*  .  */ {ASCII_PUNCT},
    /*  /  */ {ASCII_PUNCT},
    /*  0  */ {ASCII_DECIMAL | ASCII_HEXADECIMAL},
    /*  1  */ {ASCII_DECIMAL | ASCII_HEXADECIMAL},
    /*  2  */ {ASCII_DECIMAL | ASCII_HEXADECIMAL},
    /*  3  */ {ASCII_DECIMAL | ASCII_HEXADECIMAL},
    /*  4  */ {ASCII_DECIMAL | ASCII_HEXADECIMAL},
    /*  5  */ {ASCII_DECIMAL | ASCII_HEXADECIMAL},
    /*  6  */ {ASCII_DECIMAL | ASCII_HEXADECIMAL},
    /*  7  */ {ASCII_DECIMAL | ASCII_HEXADECIMAL},
    /*  8  */ {ASCII_DECIMAL | ASCII_HEXADECIMAL},
    /*  9  */ {ASCII_DECIMAL | ASCII_HEXADECIMAL},
    /*  :  */ {ASCII_PUNCT},
    /*  ;  */ {ASCII_PUNCT},
    /*  <  */ {ASCII_PUNCT},
    /*  =  */ {ASCII_PUNCT},
    /*  >  */ {ASCII_PUNCT},
    /*  ?  */ {ASCII_PUNCT},
    /*  @  */ {ASCII_PUNCT},
    /*  A  */ {ASCII_UPPER | ASCII_HEXADECIMAL},
    /*  B  */ {ASCII_UPPER | ASCII_HEXADECIMAL},
    /*  C  */ {ASCII_UPPER | ASCII_HEXADECIMAL},
    /*  D  */ {ASCII_UPPER | ASCII_HEXADECIMAL},
    /*  E  */ {ASCII_UPPER | ASCII_HEXADECIMAL},
    /*  F  */ {ASCII_UPPER | ASCII_HEXADECIMAL},
    /*  G  */ {ASCII_UPPER},
    /*  H  */ {ASCII_UPPER},
    /*  I  */ {ASCII_UPPER},
    /*  J  */ {ASCII_UPPER},
    /*  K  */ {ASCII_UPPER},
    /*  L  */ {ASCII_UPPER},
    /*  M  */ {ASCII_UPPER},
    /*  N  */ {ASCII_UPPER},
    /*  O  */ {ASCII_UPPER},
    /*  P  */ {ASCII_UPPER},
    /*  Q  */ {ASCII_UPPER},
    /*  R  */ {ASCII_UPPER},
    /*  S  */ {ASCII_UPPER},
    /*  T  */ {ASCII_UPPER},
    /*  U  */ {ASCII_UPPER},
    /*  V  */ {ASCII_UPPER},
    /*  W  */ {ASCII_UPPER},
    /*  X  */ {ASCII_UPPER},
    /*  Y  */ {ASCII_UPPER},
    /*  Z  */ {ASCII_UPPER},
    /*  [  */ {ASCII_PUNCT},
    /*  \  */ {ASCII_PUNCT},
    /*  ]  */ {ASCII_PUNCT},
    /*  ^  */ {ASCII_PUNCT},
    /*  _  */ {ASCII_PUNCT},
    /*  `  */ {ASCII_PUNCT},
    /*  a  */ {ASCII_LOWER | ASCII_HEXADECIMAL},
    /*  b  */ {ASCII_LOWER | ASCII_HEXADECIMAL},
    /*  c  */ {ASCII_LOWER | ASCII_HEXADECIMAL},
    /*  d  */ {ASCII_LOWER | ASCII_HEXADECIMAL},
    /*  e  */ {ASCII_LOWER | ASCII_HEXADECIMAL},
    /*  f  */ {ASCII_LOWER | ASCII_HEXADECIMAL},
    /*  g  */ {ASCII_LOWER},
    /*  h  */ {ASCII_LOWER},
    /*  i  */ {ASCII_LOWER},
    /*  j  */ {ASCII_LOWER},
    /*  k  */ {ASCII_LOWER},
    /*  l  */ {ASCII_LOWER},
    /*  m  */ {ASCII_LOWER},
    /*  n  */ {ASCII_LOWER},
    /*  o  */ {ASCII_LOWER},
    /*  p  */ {ASCII_LOWER},
    /*  q  */ {ASCII_LOWER},
    /*  r  */ {ASCII_LOWER},
    /*  s  */ {ASCII_LOWER},
    /*  t  */ {ASCII_LOWER},
    /*  u  */ {ASCII_LOWER},
    /*  v  */ {ASCII_LOWER},
    /*  w  */ {ASCII_LOWER},
    /*  x  */ {ASCII_LOWER},
    /*  y  */ {ASCII_LOWER},
    /*  z  */ {ASCII_LOWER},
    /*  {  */ {ASCII_PUNCT},
    /*  |  */ {ASCII_PUNCT},
    /*  }  */ {ASCII_PUNCT},
    /*  ~  */ {ASCII_PUNCT},
    /* DEL */ {ASCII_CONTROL},
};

#ifndef ASCII_NO_SET

static ascii_Set_Limb *
ascii_set_get_limb_ptr(ascii_Set *set, char c, ascii_Set_Limb *mask)
{
    ascii_Set_Limb curr, limb, finger;

    curr   = cast(ascii_Set_Limb)c;
    limb   = curr / ASCII_SET_LIMB_BITS; // byte index        e.g. 33 // 8 = 4
    finger = curr % ASCII_SET_LIMB_BITS; // bit index in byte e.g. 33 %  8 = 1
    *mask  = (1 << finger);
    return &set->limbs[limb];
}
extern ascii_Set
ascii_set_make(const char *s)
{
    size_t n = strlen(s);
    return ascii_set_make_lstring(s, n);
}

extern ascii_Set
ascii_set_make_lstring(const char *s, size_t n)
{
    ascii_Set set;
    memset(&set, 0, sizeof(set));

    // Bit packing shenanigans because C doesn't have native bitsets
    // e.g. '!' (33) goes in byte ("limb") 4, bit ("finger") 1.
    //
    // Byte |0.......|1.......|2.......|3.......|4.......|
    // Ones |01234567|89012345|67890123|45678901|23456789|
    // Tens |0.......|..1.....|....2...|......3.|........|
    //                                            ^
    //                                           '!'
    for (size_t i = 0; i < n; i += 1) {
        ascii_Set_Limb *limb;
        ascii_Set_Limb mask;

        limb   = ascii_set_get_limb_ptr(&set, s[i], &mask);
        *limb |= mask; // Toggle the bit for this character in the bitset
    }
    return set;
}

extern ascii_Set
ascii_set_union(ascii_Set a, ascii_Set b)
{
    ascii_Set u;
    for (size_t i = 0; i < ASCII_SET_LIMB_COUNT; i += 1) {
        u.limbs[i] = a.limbs[i] | b.limbs[i];
    }
    return u;
}

extern ascii_Set
ascii_set_intersection(ascii_Set a, ascii_Set b)
{
    ascii_Set n;
    for (size_t i = 0; i < ASCII_SET_LIMB_COUNT; i += 1) {
        n.limbs[i] = a.limbs[i] & b.limbs[i];
    }
    return n;
}

extern bool
ascii_set_contains(ascii_Set set, char c)
{
    ascii_Set_Limb limb, mask;
    limb = *ascii_set_get_limb_ptr(&set, c, &mask);
    return (limb & mask) != 0;
}

#endif // !ASCII_NO_SET

static bool
ascii_has_traits(char c, uint8_t t)
{
    return (ASCII_TRAITS[c].traits & t) != 0;
}

extern bool
ascii_is_control(char c)
{
    return ascii_has_traits(c, ASCII_CONTROL);
}

extern bool
ascii_is_print(char c)
{
    return !ascii_is_control(c);
}

extern bool
ascii_is_space(char c)
{
    return ascii_has_traits(c, ASCII_SPACE);
}

extern bool
ascii_is_blank(char c)
{
    return ascii_has_traits(c, ASCII_BLANK);
}

extern bool
ascii_is_punct(char c)
{
    return ascii_has_traits(c, ASCII_PUNCT);
}

extern bool
ascii_is_lower(char c)
{
    return ascii_has_traits(c, ASCII_LOWER);
}

extern bool
ascii_is_upper(char c)
{
    return ascii_has_traits(c, ASCII_UPPER);
}

extern bool
ascii_is_decimal(char c)
{
    return ascii_has_traits(c, ASCII_DECIMAL);
}

extern bool
ascii_is_hexadecimal(char c)
{
    return ascii_has_traits(c, ASCII_HEXADECIMAL);
}

extern bool
ascii_is_alnum(char c)
{
    return ascii_has_traits(c, ASCII_ALNUM);
}

extern bool
ascii_is_letter(char c)
{
    return ascii_has_traits(c, ASCII_LETTER);
}

extern bool
ascii_is_graph(char c)
{
    return ascii_has_traits(c, ASCII_GRAPH);
}

#if 0

#include <stdio.h>

// Assumes `buf` is more than enough even in the worst case!
static int
convert_base(unsigned int n, unsigned int base, char *buf)
{
    char *it;
    unsigned int value, power, digit;

    it    = buf;
    value = n;
    power = 1;
    // Get the largest power nearest to the value, but not greater.
    while (power * base < value) {
        power *= base;
    }

    while (power > 0) {
        digit  = value / power;
        value %= power;
        power /= base;
        *it++  = cast(char)(digit + '0');
    }
    return cast(int)(it - buf);
}

// Assumes `buf` is more than enough even in the worst case!
static void
quote_char(char c, char *buf)
{
    char *it;

    it    = buf;
    *it++ = '\'';
    *it++ = '\\';
    switch (c) {
    // Common C-style escapes
    case '\a': *it++ = 'a'; break;
    case '\b': *it++ = 'b'; break;
    case '\t': *it++ = 't'; break;
    case '\n': *it++ = 'n'; break;
    case '\v': *it++ = 'v'; break;
    case '\f': *it++ = 'f'; break;
    case '\r': *it++ = 'r'; break;
    default:
        if (ascii_is_control(c)) {
            it += convert_base(cast(unsigned int)c, /*base=*/8, it);
        } else {
            // Overwrite the '\\' character.
            it[-1] = c;
        }
        break;
    }
    *it++ = '\'';
    *it++ = 0;
}

int main(void)
{
    ascii_Set space = ascii_set_make("\t\n\v\f\r ");
    // Char itself can't represent 128 and thus can't compare against it.
    for (size_t i = 0; i < sizeof(ASCII_TRAITS); i += 1) {
        char buf[8];
        quote_char(cast(char)i, buf);
        printf("%-6s | Traits{%#x}\n", buf, ASCII_TRAITS[i].traits);
    }

    printf("--------\n");
    for (size_t i = 0; i < sizeof(ASCII_TRAITS); i += 1) {
        char c = cast(char)i;
        // Confirm that our set is conceptually the same as the traits
        if (ascii_set_contains(space, c) && ascii_is_space(c)) {
            char buf[8];
            quote_char(c, buf);
            printf("%-6s\n", buf);
        }
    }
    return 0;
}

#endif
