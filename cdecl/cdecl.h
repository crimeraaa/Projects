#ifndef CDECL_H
#define CDECL_H

// standard
#include <stdint.h> // u?int[\d]+_t

// local
#include "../common.h"

enum Type_Kind_Basic {
    TYPE_VOID,
    TYPE_BOOL,

    // Integral types. When not otherwise specified, all integer types,
    // (sans `char`) are signed. `char` is distinct from its counterparts.
    TYPE_CHAR,
    TYPE_SCHAR, TYPE_SHORT,  TYPE_INT,  TYPE_LONG,  TYPE_LLONG,
    TYPE_UCHAR, TYPE_USHORT, TYPE_UINT, TYPE_ULONG, TYPE_ULLONG,

    // Floating point types.
    TYPE_FLOAT, TYPE_DOUBLE, TYPE_LDOUBLE,
};

#define TYPE_KIND_BASIC_COUNT   (TYPE_LDOUBLE + 1)

enum Type_Info_Variant_Kind {
    TYPE_VKIND_VOID,
    TYPE_VKIND_BOOL,
    TYPE_VKIND_INTEGER,
    TYPE_VKIND_FLOATING,
    TYPE_VKIND_POINTER,  // Pointer types. Subtype must be realized.

    // User-defined types.
    TYPE_VKIND_STRUCT,
    TYPE_VKIND_UNION,
    TYPE_VKIND_ENUM,
};

enum Type_Qual_Flag {
    // Ignore `restrict` (C99, C++ extension) & `_Atomic` (C11).
    TYPE_QUAL_CONST     = 1 << 0,
    TYPE_QUAL_VOLATILE  = 1 << 1,
};

struct Type_Info;

struct Type_Info_Integer {
    bool is_signed;
};

struct Type_Info_Pointer {
    struct Type_Info *pointee;
};

struct Type_Info {
    // Base information.
    size_t size, align;

    // Bitset. Use `Type_Qual_Flag` as bit flags.
    uint8_t quals;

    // Base information: canonical name.
    // Opinionated at times, but it's something...
    const char *name;

    // Variant
    enum Type_Info_Variant_Kind kind;
    union {
        struct Type_Info_Integer integer;
        struct Type_Info_Pointer pointer;
    };
};

enum Type_Error {
    TYPE_ERROR_NONE,

    // Not necessarily an error!
    TYPE_ERROR_EOF,

    // Parsing errors
    TYPE_ERROR_UNEXPECTED_TOKEN,
    TYPE_ERROR_UNKNOWN_IDENTIFIER,

    // Allocation errors
    TYPE_ERROR_OUT_OF_MEMORY,
    TYPE_ERROR_INVALID_ALLOCATOR,
};

extern const struct Type_Info
TYPE_INFO_BASIC[TYPE_KIND_BASIC_COUNT];

extern enum Type_Error
cdecl_parse(const char *s, size_t n, const struct Type_Info **p);

#endif // !CDECL_H
