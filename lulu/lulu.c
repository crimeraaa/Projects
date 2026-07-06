#include <stddef.h>  // size_t
#include <stdio.h>   // fputc, fputs, printf, fgets
#include <string.h>  // strcspn

#include "api.c"

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

extern int
main(void)
{
    lulu_State *L;

    L = lulu_open();
    if (!L) {
        fprintf(stderr, "Failed to create main Lulu state\n");
        return 1;
    }

    for (;;) {
        char *     s;
        size_t     n;
        lulu_Error err;

        s = get_string_fixed(&n);
        if (!s) {
            fputc('\n', stdout);
            break;
        }

        n    = strcspn(s, "\r\n");
        s[n] = 0;
        err  = lulu_compile(L, "stdin", s, n);
        if (err) {
            printf("%s\n", lulu_error_string(err));
        }
    }

    lulu_close(L);
    return 0;
}

