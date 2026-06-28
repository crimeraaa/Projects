#include <stdio.h>
#include <string.h>

#include "../common.h"
#include "lexer.c"

#define PROMPT ">>> "

internal char *
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

internal void
run(const char *s, size_t n)
{
    Lexer x = lexer_make(string_make(s, n));
    for (;;) {
        Token t;
        lulu_Error err;
        err = lexer_scan_token(&x, &t);
        if (err) {
            if (err != LULU_EOF) {
                fprintf(stderr, "[ERROR]: %s\n", lulu_error_string(err));
            }
            break;
        }

        printf("Token_Kind(%i) | '%.*s'\n",
            t.kind,
            cast(int)t.lexeme.len, t.lexeme.data);
    }
}

global int
main(void)
{
    char buf[256];
    for (;;) {
        char *s;
        size_t n;

        s = get_string_fixed(buf, sizeof(buf), &n);
        if (!s) {
            fputc('\n', stdout);
            break;
        }
        n    = strcspn(s, "\r\n");
        s[n] = 0;
        run(s, n);
    }
    return 0;
}

global const char *
lulu_error_string(lulu_Error err)
{
    switch (err) {
    case LULU_OK:                   return "no error";
    case LULU_EOF:                  return "end of file";
    case LULU_UNEXPECTED_CHARACTER: return "unexpected character";
    case LULU_INVALID_NUMBER:       return "invalid number";
    case LULU_UNTERMINATED_STRING:  return "unterminated string";
    case LULU_UNEXPECTED_TOKEN:     return "unexpected token";
    case LULU_RUNTIME_ERROR:        return "runtime error";
    }
    return NULL;
}
