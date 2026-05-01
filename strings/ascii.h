#ifndef PROJECT_STRINGS_ASCII_H
#define PROJECT_STRINGS_ASCII_H

// standard
#include <limits.h> // CHAR_BIT
#include <stdint.h> // u?int[\d]+_t, u?intptr_t

#include "../common.h"

// You can disable this type if you don't want it around,
// e.g. in unity builds.
#ifndef ASCII_NO_SET

#define ASCII_SET_BITS       128
#define ASCII_SET_LIMB_TYPE  uint32_t
#define ASCII_SET_LIMB_BITS  (sizeof(ascii_Set_Limb) * CHAR_BIT)
#define ASCII_SET_LIMB_COUNT (ASCII_SET_BITS / ASCII_SET_LIMB_BITS)
typedef ASCII_SET_LIMB_TYPE  ascii_Set_Limb;


// 128-bit bitset
typedef struct ascii_Set ascii_Set;
struct ascii_Set {
    ascii_Set_Limb limbs[ASCII_SET_LIMB_COUNT];
};

extern ascii_Set
ascii_set_make(const char *s);

extern ascii_Set
ascii_set_make_lstring(const char *s, size_t n);

extern ascii_Set
ascii_set_union(ascii_Set a, ascii_Set b);

extern ascii_Set
ascii_set_intersection(ascii_Set a, ascii_Set b);

extern bool
ascii_set_contains(ascii_Set set, char c);

#endif // !ASCII_NO_SET

extern bool
ascii_is_control(char c);

extern bool
ascii_is_print(char c);

extern bool
ascii_is_space(char c);

extern bool
ascii_is_blank(char c);

extern bool
ascii_is_punct(char c);

extern bool
ascii_is_lower(char c);

extern bool
ascii_is_upper(char c);

extern bool
ascii_is_decimal(char c);

extern bool
ascii_is_hexadecimal(char c);

extern bool
ascii_is_letter(char c);

extern bool
ascii_is_alnum(char c);

extern bool
ascii_is_graph(char c);


#endif // !PROJECT_STRINGS_ASCII_H
