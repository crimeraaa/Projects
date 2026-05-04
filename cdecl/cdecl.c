#if 1
#define MEM_IMPLEMENTATION
#endif

#include <stdio.h>      // printf

#include "lexer.c"
#include "table.c"
#include "parser.c"

static Table cdecl_symbol_table;

// Set up the internal global symbol table.
extern Type_Error
cdecl_init(mem_Allocator alloc)
{
    return table_init(&cdecl_symbol_table, alloc);
}

// Clean up the internal global symbol table.
extern void
cdecl_destroy(void)
{
    table_destroy(&cdecl_symbol_table);
}

extern const Type_Info *
cdecl_parse(const char *s, size_t n, Type_Error *out)
{
    Parser p;
    Type_Error err;
    if (out == NULL) {
        out = &err;
    }
    
    *out = parser_init(&p, s, n, &cdecl_symbol_table);
    if (*out) {
        return NULL;
    }
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
    Type_Error err = cdecl_init(mem_global_heap_allocator());
    if (err) {
        printf("Type_Error(%i): Failed to initialize the global symbol table!\n", err);
        return 1;
    }

    for (;;) {
        const Type_Info *p;
        const char *input;
        size_t input_len;
        char buf[256];

        printf("cdecl>");
        input = get_string(buf, sizeof(buf), &input_len);
        if (input == NULL) {
            printf("\n");
            break;
        }

        p = cdecl_parse(input, input_len, &err);
        if (err) {
            printf("\n");
            break;
        }
    }
    cdecl_destroy();
    return 0;
}

#endif
