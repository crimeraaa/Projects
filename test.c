// stfu microslop
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#endif

#include <stdio.h>  // printf

// Only needed for the default global heap allocator (malloc-based).
// #define MEM_IMPLEMENTATION
#define MEM_ARENA_IMPLEMENTATION
#define STRINGS_BUILDER_IMPLEMENTATION
#include "mem/mem.h"
#include "mem/arena.h"
#include "strings/builder.h"

internal void
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

typedef struct {
    // 1D array of non-owning views into some giant string.
    string_View *data;
    size_t len;
} File_Contents;

internal mem_Allocator_Error
read_file_by_lines(const char *name, mem_Allocator allocator)
{
    FILE *f;
    char *contents = NULL;
    size_t n = 0;
    mem_Allocator_Error err = MEM_OK;

    f = fopen(name, "r");
    if (!f) {
        fprintf(stderr, "Failed to open file '%s'.\n", name);
        return MEM_INVALID_ARGUMENT;
    }

    for (;;) {
        char buf[BUFSIZ];
        string_View line;
        line.data = fgets(buf, sizeof(buf), f);
        if (!line.data) {
            break;
        }
        line = string_make_cstring(line.data);
        // This line is complete?
        if (string_index_char(line, '\n')) {
            // ...
        }
        // Otherwise, the line is not yet complete.
        else {

        }
    }

    fclose(f);
    return err;
}

global int
main(int argc, char *argv[])
{
    string_Builder b;
    mem_Allocator allocator;
    mem_Arena arena;
    char buf[4096];

    mem_arena_init(&arena, buf, sizeof(buf));
    allocator = mem_arena_allocator(&arena);
    string_builder_init_none(&b, allocator);
    return 0;
}

#if 0

internal void
read_string(string_Builder *b, const char *prompt)
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
            return;
        }
    }
    printf("[ERROR] No input received. Default: empty string.\n");
}

internal int
parse_int(string s, bool *ok)
{
    char *end;
    ullong u;
    llong sign = 1;
    int base = 10;

    // `strto*()` can't handle negative prefixed integers, e.g. `-0xff`.
    for (; s.len > 0; s = string_slice_from(s, 1)) {
        switch (s.data[0]) {
        case '-': sign *= -1;
        case '+': continue;
        }
        break;
    }

    if (s.len > 2 && s.data[0] == '0') {
        // Sentinel value. There is no such thing as a base-0 number.
        base = 0;
        switch (s.data[1]) {
        case 'b': case 'B': base = 2;  break;
        case 'o': case 'O': base = 8;  break;
        case 'd': case 'D': base = 10; break;
        case 'z': case 'Z': base = 12; break;
        case 'x': case 'X': base = 16; break;
        }

        if (base != 0) {
            // Skip the prefix because `strto*()` can't handle most of them.
            s = string_slice_from(s, 2);
        } else {
            base = 10;
        }
    }
    u   = strtoull(s.data, &end, base);
    *ok = (end == s.data + s.len) && (u <= cast(ullong)INT_MAX + 1);
    return (*ok) ? cast(int)(cast(llong)u * sign) : INT_MIN;
}

internal void
read_int(string_Builder *b, const char *prompt)
{
    int i;
    char buf[BUFSIZ];
    if (prompt != NULL) {
        printf("%s", prompt);
    }

    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        // Useful to test our int writing implementation.
        i = INT_MIN;
        printf("[ERROR] No input received. Default: INT_MIN.\n");
    } else {
        size_t n;
        bool ok;

        // Needed for `strtoul()` to work.
        n = strcspn(buf, "\r\n");
        buf[n] = 0;

        i = parse_int(string_make(buf, n), &ok);
        if (!ok) {
            printf("[ERROR] Failed to parse integer '%s' Default: INT_MIN.\n",
                buf);
        }
    }

    // Always write the decimal representation for simplicity.
    check(string_write_int(b, i, /*base=*/10));
}

int
main(void)
{
    string_Builder b;
    string_View s;

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

#endif
