#ifndef CDECL_H
#define CDECL_H

// standard
#include <stdint.h> // u?int[\d]+_t

// local
#include "../common.h"
#include "../mem/mem.h"

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

    // Integer only, used only for table lookup.
    TYPE_QUAL_SIGNED    = 1 << 2,
    TYPE_QUAL_UNSIGNED  = 1 << 3,
};

typedef enum Type_Kind_Basic        Type_Kind_Basic;
typedef enum Type_Info_Variant_Kind Type_Info_Variant_Kind;
typedef enum Type_Qual_Flag         Type_Qual_Flag;

typedef struct Type_Info Type_Info;
struct Type_Info {
    // Base information.
    size_t size, align;

    // Base information: canonical name.
    // Opinionated at times, but it's something...
    const char *name;
    size_t name_len;

    // Variant
    enum Type_Info_Variant_Kind kind;
    // Bitset. Use `Type_Qual_Flag` as bit flags.
    uint8_t quals;

    // Variant
    union {
        bool is_signed;
        Type_Info *pointee;
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

typedef enum Type_Error Type_Error;

// Set up the internal symbol table.
extern Type_Error
cdecl_init(mem_Allocator alloc);

// Clean up the internal symbol table.
extern void
cdecl_destroy(void);

extern const Type_Info *
cdecl_parse(const char *s, size_t n, Type_Error *out);

#endif // !CDECL_H
