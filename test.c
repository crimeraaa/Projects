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
#include "mem/mem.h"
#include "mem/arena.h"
#include "strings/builder.h"

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
        check(string_write_string(b, string_make(buf, n)));
        // Newlines indicate the input is now complete.
        // Otherwise, we need to keep going until we find them.
        if (buf[n] == '\r' || buf[n] == '\n') {
            break;
        }
    }
}

// TODO: Breaks for negatives, e.g. -0xff.
static int
parse_int(const char *s, size_t n)
{
    char *end;
    int i = 0, base = 10;
    if (n > 2 && s[0] == '0') {
        // Sentinel value. There is no such thing as a base-0 number.
        base = 0;
        switch (s[1]) {
        case 'b': case 'B': base = 2;  break;
        case 'o': case 'O': base = 8;  break;
        case 'd': case 'D': base = 10; break;
        case 'z': case 'Z': base = 12; break;
        case 'x': case 'X': base = 16; break;
        }

        if (base != 0) {
            // Skip the prefix because `strtol` can't handle most of them.
            s += 2;
            n -= 2;
        } else {
            base = 10;
        }
    }

    i = cast(int)strtol(s, &end, base);
    if (end != s + n) {
        i = INT_MIN;
    }
    return i;
}

static void
read_int(String_Builder *b, const char *prompt)
{
    int i;
    char buf[BUFSIZ];
    if (prompt != NULL) {
        printf("%s", prompt);
    }

    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        // Useful to test our int writing implementation.
        i = INT_MIN;
    } else {
        size_t n = strcspn(buf, "\r\n");
        buf[n]   = 0;
        i = parse_int(buf, n);
    }

    // Always write the decimal representation for simplicity.
    check(string_write_int(b, i, /*base=*/10));
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
    string_builder_init_none(&b, mem_arena_allocator(&a));
    check(string_write_char(&b, 'H'));
    check(string_write_char(&b, 'i'));
    check(string_write_char(&b, ' '));
    check(string_write_literal(&b, "mom!"));
    check(string_write_literal(&b, " My name is "));

    read_string(&b, "Enter your name: ");
    check(string_write_literal(&b, " and I'm "));

    read_int(&b, "Enter your age: ");
    check(string_write_literal(&b, " years old"));
    check(string_write_char(&b, '!'));

    // Underlying data is guaranteed to be nul-terminated.
    s = string_to_string(&b);
    printf("'%s' (%zu bytes)\n", s.data, s.len);
    string_builder_destroy(&b);
    return 0;
}
