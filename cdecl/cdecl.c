#if 1
#define MEM_IMPLEMENTATION
#endif

#include <stdio.h>      // printf

#include "lexer.c"
#include "table.c"
#include "parser.c"
#include "table.h"

#define X(T, type_class, is_signed) {   \
    /*quals  =*/ 0,                     \
    /*size   =*/ sizeof(T),             \
    /*align  =*/ alignof(T),            \
    /*name   =*/ #T,                    \
    /*kind   =*/ type_class,            \
    /*integer=*/ {is_signed},           \
}

const struct Type_Info
TYPE_INFO_BASIC[TYPE_KIND_BASIC_COUNT] = {
    /*TYPE_VOID     =*/ {0, 0, 0, "void", TYPE_VKIND_VOID, {false}},
    /*TYPE_BOOL     =*/ X(bool,                 TYPE_VKIND_BOOL,      false),
    /*TYPE_CHAR     =*/ X(char,                 TYPE_VKIND_INTEGER,   false),
    /*TYPE_SCHAR    =*/ X(signed char,          TYPE_VKIND_INTEGER,   true),
    /*TYPE_SHORT    =*/ X(short,                TYPE_VKIND_INTEGER,   true),
    /*TYPE_INT      =*/ X(int,                  TYPE_VKIND_INTEGER,   true),
    /*TYPE_LONG     =*/ X(long,                 TYPE_VKIND_INTEGER,   true),
    /*TYPE_LLONG    =*/ X(long long,            TYPE_VKIND_INTEGER,   true),
    /*TYPE_UCHAR    =*/ X(unsigned char,        TYPE_VKIND_INTEGER,   false),
    /*TYPE_USHORT   =*/ X(unsigned short,       TYPE_VKIND_INTEGER,   false),
    /*TYPE_UINT     =*/ X(unsigned int,         TYPE_VKIND_INTEGER,   false),
    /*TYPE_ULONG    =*/ X(unsigned long,        TYPE_VKIND_INTEGER,   false),
    /*TYPE_ULLONG   =*/ X(unsigned long long,   TYPE_VKIND_INTEGER,   false),
    /*TYPE_FLOAT    =*/ X(float,                TYPE_VKIND_FLOATING,  false),
    /*TYPE_DOUBLE   =*/ X(double,               TYPE_VKIND_FLOATING,  false),
    /*TYPE_LDOUBLE  =*/ X(long double,          TYPE_VKIND_FLOATING,  false),
};

#undef X

static const struct Type_Info *
cdecl_get_basic(enum Type_Kind_Basic t)
{
    return &TYPE_INFO_BASIC[t];
}

static struct Table global_symbol_table;

extern enum Type_Error
cdecl_parse(const char *s, size_t n, const struct Type_Info **out)
{
    struct Parser p;
    parser_init(&p, s, n, &global_symbol_table);
    return parser_parse(&p, out);
}

#if 1

static const char *
get_string(char *buf, size_t buf_len, size_t *out_len)
{
    char *s = fgets(buf, buf_len, stdin);
    if (s != NULL) {
        *out_len    = strcspn(s, "\r\n");
        s[*out_len] = 0;
    }
    return s;
}

int main(void)
{
    mem_Allocator alloc;
    size_t input_len;
    const char *input;
    enum Type_Error err;
    char buf[256];

    alloc = mem_global_heap_allocator();
    err   = table_init(&global_symbol_table, alloc);
    if (err) {
        printf("Type_Error(%i): Failed to initialize the global symbol table!\n", err);
        return 1;
    }

    for (;;) {
        printf("cdecl>");
        input = get_string(buf, sizeof(buf), &input_len);
        if (input == NULL) {
            printf("\n");
            break;
        }

        err = cdecl_parse(input, input_len, NULL);
        if (err) {
            printf("\n");
            break;
        }
    }
    table_destroy(&global_symbol_table);
    return 0;
}

#endif
