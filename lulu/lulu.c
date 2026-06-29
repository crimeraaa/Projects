#include <stdio.h>
#include <string.h>

#include "state.c"
#include "parser.c"
#include "api.c"

#define PROMPT ">>> "

static char *
get_string_fixed(char *buf, size_t buf_len, size_t *line_len)
{
    char *line;

    fputs(PROMPT, stdout);
    // Why does this standard function take an int? Are you stupid?
    line      = fgets(buf, cast(int)buf_len, stdin);
    *line_len = 0;
    if (line) {
        *line_len       = strcspn(line, "\r\n");
        line[*line_len] = 0;
    }
    return line;
}

static void
run(lulu_State *L, const char *s, size_t n, Arena *arena)
{
    Ast_Node *root = parser_parse(L, "stdin", s, n, arena);
    ast_print(root);
}

static void
wrap(lulu_State *L, void *user_data)
{
    Arena arena;
    char line_buf[256];
    char arena_buf[4096];

    unused(user_data);
    arena = arena_make(arena_buf, sizeof(arena_buf));
    for (;;) {
        char *s;
        size_t n;

        s = get_string_fixed(line_buf, sizeof(line_buf), &n);
        if (!s) {
            fputc('\n', stdout);
            break;
        }
        n    = strcspn(s, "\r\n");
        s[n] = 0;
        run(L, s, n, &arena);
        arena_free_all(&arena);
    }
}

extern int
main(void)
{
    lulu_State *L = lulu_open();
    lulu_Error err;

    err = state_try(L, wrap, NULL);
    if (err) {
        printf("%s\n", lulu_error_string(err));
    }
    return 0;
}

