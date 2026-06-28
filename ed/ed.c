// standard
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <stdarg.h> // va_list
#include <stdio.h> // fgets, stdin, stdout

// local
#define MEM_DEF internal
#define MEM_IMPLEMENTATION
#define MEM_ARENA_IMPLEMENTATION
#include "../mem/arena.h"

#define STRINGS_DEF internal
#define STRINGS_IMPLEMENTATION
#define STRINGS_BUILDER_DEF internal
#define STRINGS_BUILDER_IMPLEMENTATION
#include "../strings/builder.h"

#include "ed.h"

internal void
ed_run(ed_State *ed, string_View line)
{
    size_t i = 0;
    /** @brief 2. Look for the command proper.
     * @link https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ed.html
     */
    switch (line.data[i]) {
    case 'a':
    case 'p':
    default:
        ed_error(ed, "Command '%c' not implemented.", line.data[i]);
        break;
    }
}

internal bool
ed_get_line(ed_State *ed, string_Builder *b, string_View *line)
{
    for (;;) {
        mem_Allocator_Error err;
        int c = fgetc(stdout);
        // For our sanity, don't write the trailing newline.
        switch (c) {
        case EOF:
            return false;
        case '\r':
        case '\n':
            *line = string_to_string(b);
            return true;
        }

        err = string_write_char(b, c);
        if (err) {
            if (ed->help) {
                fprintf(stderr,
                    "Command line maximum is %zu characters.",
                    b->cap);
            }
            return false;
        }
    }
    // Unreachable.
    return false;
}

global int
main(int argc, char *argv[])
{
    ed_State ed;
    string_Builder b;
    mem_Arena arena;
    char buf[256];

    // TODO: Allow `-p` flag from CLI? Since it's always nul-terminated,
    //       we don't need to worry about allocations.
    ed_init(&ed, "*", mem_global_heap_allocator());
    mem_arena_init(&arena, buf, sizeof(buf));
    string_builder_init_cap(&b, sizeof(buf), mem_arena_allocator(&arena));
    for (;;) {
        string_View line;
        if (ed.prompt) {
            fputs(ed.prompt, stdout);
        }

        string_builder_reset(&b);
        // EOF or memory error occured?
        if (!ed_get_line(&ed, &b, &line)) {
            if (ed.prompt) {
                fputc('\n', stdout);
            }
            break;
        }
        ed_run(&ed, line);
    }
    return 0;
}

global void
ed_init(ed_State *ed, const char *prompt, mem_Allocator allocator)
{
    ed->allocator = allocator;
    ed->prompt    = prompt;
    ed->help      = true;
}

global void
(ed_error)(ed_State *ed, const char *file, int line, const char *fmt, ...)
{
    // lol
    fputs("?\n", stderr);
    if (ed->help) {
        va_list args;
        va_start(args, fmt);
        fprintf(stderr, "%s:%i: ", file, line);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

global int
ed_read_file(ed_State *ed, const char *name, size_t *size)
{
    FILE *f = NULL;
    int line_count = 0;

    f     = fopen(name, "r");
    *size = 0;
    if (!f) {
        ed_error(ed, "Failed to read file '%s'.", name);
        return -1;
    }

    for (;;) {
        size_t line_len;
        char *line = ed_read_stream_line(ed, f, &line_len);
        if (!line) {
            break;
        }

        // We use malloc so this is fine even if we don't pass
        // in the string builder's capacity.
        mem_free_array(ed->allocator, line, line_len, NULL);
    }

    fclose(f);
    return line_count;
}

global char *
ed_read_stream_line(ed_State *ed, FILE *f, size_t *len)
{
    string_Builder b;
    mem_Allocator_Error err = MEM_OK;
    int c = 0;

    *len = 0;
    string_builder_init_none(&b, ed->allocator);
    for (;;) {
        c = fgetc(f);
        if (c == EOF || c == '\n') {
            break;
        }
        // On Windows, if we find CR ('\r'), we might have CRLF ("\r\n").
        else if (c == '\r') {
            c = fgetc(f);
            // Didn't get CRLF, so we need to return whatever we just read
            // to ensure the next `fgetc()` call returns it again.
            if (c != '\n') {
                ungetc(c, f);
            }
            break;
        }

        err = string_write_char(&b, c);
        if (err) {
            fputs("[FATAL]: realloc failed!\n", stderr);
            exit(2);
        }
    }

    // Since we're using malloc anyway, we don't need to return
    // the allocated capacity.
    *len = b.len;
    return b.data;
}
