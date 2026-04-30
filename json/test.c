#include <stdio.h>  // printf
#include <string.h> // strlen

#define MEM_IMPLEMENTATION
#include "lexer.c"
#include "parser.c"
#include "json.c"


int main(int argc, char *argv[])
{
    mem_Allocator allocator;
    json_Value v;
    json_Error err;

    allocator = mem_global_heap_allocator();
    if (argc == 1) {
        char s[] = "{\n"
"   \"\\u0068\\u0069\": \"\\u006d\\u006f\\u006d\"\n,"
"   \"\\u006d\\u006f\\u006d\": \"\\u0068\\u0069\"\n"
"}\n";
        err = json_parse_lstring(s, sizeof(s) - 1, allocator, &v);
    } else {
        const char *s;
        size_t n;

        s   = argv[1];
        n   = strlen(s);
        err = json_parse_lstring(s, n, allocator, &v);
    }
    if (!err) {
        json_print_value(v);
        printf("\n");
        json_destroy_value(v, allocator);
    } else {
        printf("error: %s", json_error_string(err));
    }
    return 0;
}
