// stfu microslop
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <string.h> // memcpy, strlen
#include <stdlib.h> // strtol
#include <stdio.h>  // printf

// Only needed for the default global heap allocator (malloc-based).
// #define MEM_IMPLEMENTATION
#define MEM_ARENA_IMPLEMENTATION
#define STRINGS_BUILDER_IMPLEMENTATION
#include "mem.h"
#include "arena.h"
#include "../strings/builder.h"

static void
check(const char *file, int line, mem_Allocator_Error err)
{
    const char *info = NULL;
    switch (err) {
    case MEM_OK:
        break;
    case MEM_OUT_OF_MEMORY:
        info = "Allocator has run out of memory";
        break;
    case MEM_NOT_IMPLEMENTED:
        info = "Allocator does not implement the requested mode";
        break;
    case MEM_INVALID_ARGUMENT:
        info = "Allocator received an invalid argument";
        break;
    }
    if (info != NULL) {
        printf("%s:%i: %s\n", file, line, info);
    }
}

#define check(err)  (check)(__FILE__, __LINE__, err)

static void
read_string(String_Builder *b, const char *prompt)
{
    char buf[BUFSIZ];

    if (prompt != NULL) {
        printf("%s", prompt);
    }

    while (fgets(buf, sizeof(buf), stdin) != NULL) {
        size_t n = strcspn(buf, "\r\n");
#if 0
        printf("n=%zu{", n);
        for (size_t i = 0; i < n; i++) {
            if (i > 0) {
                printf(", ");
            }
            printf("'%c'", buf[i]);
        }
        printf("}\n");
#endif
        check(sb_write_string(b, string_make(buf, n)));
        // Newlines indicate the input is now complete.
        // Otherwise, we need to keep going until we find them.
        if (buf[n] == '\r' || buf[n] == '\n') {
            break;
        }
    }
}

static void
read_int(String_Builder *b, const char *prompt)
{
    char *end;
    size_t n;
    int i;
    char buf[BUFSIZ];

    if (prompt != NULL) {
        printf("%s", prompt);
    }

    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        // Useful to test our int writing implementation.
        i = INT_MIN;
    } else {
        n      = strcspn(buf, "\r\n");
        buf[n] = 0;
        i      = cast(int)strtol(buf, &end, /*radix=*/10);
    }
    check(sb_write_int(b, i, /*base=*/10));
}

int
main(void)
{
    String_Builder b;
    String s;

    // memory
    mem_Arena a;
    char buf[BUFSIZ];
    mem_arena_init(&a, buf, sizeof(buf));
    sb_init_none(&b, mem_arena_allocator(&a));
    check(sb_write_char(&b, 'H'));
    check(sb_write_char(&b, 'i'));
    check(sb_write_char(&b, ' '));
    check(sb_write_literal(&b, "mom!"));
    check(sb_write_literal(&b, " My name is "));

    read_string(&b, "Enter your name: ");
    check(sb_write_literal(&b, " and I'm "));

    read_int(&b, "Enter your age: ");
    check(sb_write_literal(&b, " years old"));
    check(sb_write_char(&b, '!'));

    // Underlying data is guaranteed to be nul-terminated.
    s = sb_to_string(&b);
    printf("'%s' (%zu bytes)\n", s.data, s.len);
    sb_destroy(&b);
    return 0;
}
