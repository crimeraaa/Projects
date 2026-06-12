// stfu microslop
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <string.h> // memcpy, strlen
#include <stdio.h>  // printf

// Only needed for the default global heap allocator (malloc-based).
// #define MEM_IMPLEMENTATION
#define MEM_ARENA_IMPLEMENTATION
#include "mem.h"
#include "arena.h"

#define DYNAMIC_MIN_SIZE    8

typedef struct String String;
struct String {
    const char *data;
    size_t len;
};

typedef struct String_Builder String_Builder;
struct String_Builder {
    mem_Allocator allocator;
    char *data;
    size_t len;
    size_t cap;
};

void
sb_init_none(String_Builder *b, mem_Allocator allocator)
{
    b->allocator = allocator;
    b->data      = NULL;
    b->len       = 0;
    b->cap       = 0;
}

mem_Allocator_Error
sb_init_cap(String_Builder *b, mem_Allocator allocator, size_t cap)
{
    mem_Allocator_Error err = MEM_OK;
    b->allocator = allocator;
    b->data   = mem_alloc_array(char, allocator, cap, &err);
    b->len        = 0;
    b->cap        = (b->data != NULL) ? cap : 0;
    return err;
}

mem_Allocator_Error
sb_destroy(String_Builder *b)
{
    mem_Allocator_Error err = MEM_OK;
    mem_free_array(b->allocator, b->data, b->cap, &err);
    if (err == MEM_OK) {
        b->data = NULL;
        b->len  = 0;
        b->cap  = 0;
    }
    return err;
}

mem_Allocator_Error
sb_write_char(String_Builder *b, char c)
{
    mem_Allocator_Error err = MEM_OK;

    // Appending this character plus the nul char would overflow?
    if (b->len + 2 > b->cap) {
        char *new_data;
        size_t new_cap;

        new_cap  = (b->cap > DYNAMIC_MIN_SIZE) ? b->cap * 2 : DYNAMIC_MIN_SIZE;
        new_data = mem_resize_array(char, b->allocator, b->data, b->cap, new_cap, &err);
        if (new_data == NULL) {
            return err;
        }
        b->data = new_data;
        b->cap  = new_cap;
    }

    b->len += 1;
    b->data[b->len - 1] = c;
    b->data[b->len]   = '\0';
    return err;
}

mem_Allocator_Error
sb_write_string(String_Builder *b, String s)
{
    mem_Allocator_Error err = MEM_OK;
    size_t new_len = b->len + s.len;

    // Plus one for the nul char.
    if (new_len + 1 > b->cap) {
        char *new_data;
        size_t new_cap;

        new_cap = b->cap * 2;
        if (new_len + 1 > new_cap) {
            new_cap = new_len + 1;
        }

        // New capacity is not a power of 2?
        if ((new_cap & (new_cap - 1)) != 0) {
            size_t tmp = 1;
            while (tmp < new_cap) {
                tmp *= 2;
            }
            new_cap = tmp;
        }

        new_data = mem_resize_array(char, b->allocator, b->data, b->cap, new_cap, &err);
        if (new_data == NULL) {
            return err;
        }

        b->data = new_data;
        b->cap  = new_cap;
    }

    memcpy(&b->data[b->len], s.data, s.len);
    b->data[new_len] = '\0';
    b->len           = new_len;
    return err;
}

mem_Allocator_Error
sb_write_cstring(String_Builder *b, const char *s)
{
    String s2 = {s, strlen(s)};
    return sb_write_string(b, s2);
}

#define sb_write_literal(b, lit)    sb_write_string(b, (String){lit, sizeof(lit) - 1})

char
sb_pop_char(String_Builder *b)
{
    size_t n, i;
    char c;

    // Nothing to do?
    n = b->len;
    if (n == 0) {
        return '\0';
    }

    // Read last valid character.
    i = n - 1;
    c = b->data[i];

    // Update, ensuring nul-termiantion.
    b->data[i] = '\0';
    b->len      = i;
    return c;
}

const String
sb_to_string(const String_Builder *b)
{
    String s = {b->data, b->len};
    return s;
}

static void
check(const char *file, int line, mem_Allocator_Error err)
{
    if (err == MEM_OK) {
        return;
    }

    printf("%s:%i: ", file, line);
    switch (err) {
    case MEM_OK:
        break;
    case MEM_OUT_OF_MEMORY:
        printf("Allocator has run out of memory");
        break;
    case MEM_NOT_IMPLEMENTED:
        printf("Allocator does not implement the requested mode");
        break;
    case MEM_INVALID_ARGUMENT:
        printf("Allocator received an invalid argument");
        break;
    }
    printf("\n");
}

#define check(err)  (check)(__FILE__, __LINE__, err)

static String
sb_read_string(String_Builder *b)
{
    char buf[BUFSIZ];
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
        String s = {buf, n};
        check(sb_write_string(b, s));
        // Newlines indicate the input is now complete.
        // Otherwise, we need to keep going until we find them.
        if (buf[n] == '\r' || buf[n] == '\n') {
            break;
        }
    }
    return sb_to_string(b);
}

int
main(void)
{
    String_Builder b;
    String s;

    mem_Arena a;
    char buf[BUFSIZ];
    mem_arena_init(&a, buf, sizeof(buf));

    sb_init_none(&b, mem_arena_allocator(&a));
    check(sb_write_char(&b, 'H'));
    check(sb_write_char(&b, 'i'));
    check(sb_write_char(&b, ' '));
    check(sb_write_literal(&b, "mom!"));
    check(sb_write_literal(&b, " My name is "));

    printf("Enter your name: ");
    s = sb_read_string(&b);
    check(sb_write_char(&b, '!'));
    printf("'%s' (%zu bytes)\n", s.data, s.len);

    sb_destroy(&b);
    return 0;
}
