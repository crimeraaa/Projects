#include <stdio.h>
#include <string.h>

#include "../common.h"

#define PROMPT ">>> "

internal const char *
get_string_fixed(char *buf, size_t buf_len, size_t *line_len)
{
    char *line;

    fputs(PROMPT, stdout);
    line = fgets(buf, buf_len, stdin);
    if (line != NULL) {
        *line_len       = strcspn(line, "\r\n");
        line[*line_len] = 0;
    }
    return line;
}

global int
main(void)
{
    char buf[256];
    for (;;) {
        const char *line;
        size_t line_len;

        line = get_string_fixed(buf, sizeof(buf), &line_len);
        if (line == NULL) {
            fputc('\n', stdout);
            break;
        }
    }
    return 0;
}
