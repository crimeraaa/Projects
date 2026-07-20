#define _CRT_SECURE_NO_WARNINGS
#include "lulu.h"
#include <stddef.h>  // size_t
#include <stdio.h>   // fputc, fputs, printf, fgets
#include <string.h>  // strcspn

#include "api.cpp"

#define PROMPT ">>> "

static char *
get_string_fixed(size_t *n)
{
    static char buf[256];
    char *s;

    fputs(PROMPT, stdout);
    // Why does this standard function take an int? Are you stupid?
    s  = fgets(buf, cast(int)sizeof(buf), stdin);
    *n = 0;
    if (s) {
        *n    = strcspn(s, "\r\n");
        s[*n] = 0;
    }
    return s;
}

static char *
load_file(lulu_State *L, char const *path, size_t *len)
{
    FILE * f  = fopen(path, "rb");
    long   ft = 0;
    char * s  = nullptr;
    size_t fr = 0;

    *len = 0;
    if (!f) {
        printf("Failed to open file '%s'.\n", path);
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    ft = ftell(f);
    if (ft == -1) {
        printf("Failed to determine file size of '%s' (ftell=%li).\n", path, ft);
        goto cleanup_file;
    }
    *len = cast(usize)ft;
    rewind(f);

    // Assume it doesn't fail
    s       = cast(char *)malloc(*len + 1);
    s[*len] = 0;

    fr = fread(s, sizeof(char), *len, f);
    if (fr != *len) {
        printf("Failed to properly read contents of file '%s'. (fread=%zu, len=%zu)\n", path, fr, *len);
        free(s);
        s = nullptr;
        goto cleanup_file;
    }

cleanup_file:
    fclose(f);
    return s;
}

static void
run_input(lulu_State *L, char const *path, char const *input, size_t len)
{
    lulu_Error err = lulu_compile(L, path, input, len);
    if (err) {
        printf("%s\n", lulu_error_string(err));
    }
}

static void
run_interactive(lulu_State *L)
{
    for (;;) {
        size_t n;
        char * s = get_string_fixed(&n);
        if (!s) {
            fputc('\n', stdout);
            break;
        }

        n    = strcspn(s, "\r\n");
        s[n] = 0;
        run_input(L, "stdin", s, n);
    }
}

extern int
main(int argc, char *argv[])
{
    lulu_State *L = lulu_open();
    if (!L) {
        fprintf(stderr, "Failed to create main Lulu state\n");
        return 1;
    }

    if (argc == 2) {
        size_t n;
        char * s = load_file(L, argv[1], &n);
        if (!s) {
            printf("Failed to open file '%s'.\n", argv[1]);
            return 1;
        }
        run_input(L, argv[1], s, n);
    } else {
        run_interactive(L);
    }

    lulu_close(L);
    return 0;
}

