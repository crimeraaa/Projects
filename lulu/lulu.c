#include <stdio.h>
#include <string.h>

#include "api.c"

#define PROMPT ">>> "

static char *
get_string_fixed(char *buf, size_t buf_len, size_t *n)
{
    char *s;

    fputs(PROMPT, stdout);
    // Why does this standard function take an int? Are you stupid?
    s  = fgets(buf, cast(int)buf_len, stdin);
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
    lulu_State *  L;
    unsigned char arena_buf[4096];

    L = lulu_open(arena_buf, sizeof(arena_buf));
    for (;;) {
        char *     s;
        size_t     n;
        lulu_Error err;
        char       line_buf[256];

        s = get_string_fixed(line_buf, sizeof(line_buf), &n);
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

    return 0;
}

