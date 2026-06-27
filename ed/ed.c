// standard
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <stdarg.h> // va_list
#include <stdio.h> // fgets, stdin, stdout

// local
#include "../common.h"

#define MEM_DEF internal
#define MEM_IMPLEMENTATION
#define MEM_ARENA_IMPLEMENTATION
#include "../mem/arena.h"

#define STRINGS_DEF internal
#define STRINGS_IMPLEMENTATION
#define STRINGS_BUILDER_DEF internal
#define STRINGS_BUILDER_IMPLEMENTATION
#include "../strings/builder.h"

// A mutable `char` array that does not remember its allocator.
// Each line is an independent allocation to allow in-place editing.
typedef struct ed_Line ed_Line;
struct ed_Line {
    char  *data;
    size_t len;
    size_t cap;
};

// A 1-dimensional array of `ed_Line` that represents the contents
// of the current file.
typedef struct ed_Buffer ed_Buffer;
struct ed_Buffer {
    ed_Line *data;
    // The number of lines we are actively using in `data`.
    size_t len;

    // The total number of lines we can have in `data` before a
    // reallocation needs to occur.
    size_t cap;
};

typedef struct ed_State ed_State;
struct ed_State {
    // The current file's contents.
    ed_Buffer buffer;
    mem_Allocator allocator;
    const char *prompt;
    bool help;
};

internal void
ed_buffer_init(ed_Buffer *b)
{
    b->data = NULL;
    b->len  = 0;
    b->cap  = 0;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((__format__(printf, 2, 3)))
#endif
internal void
ed_error(ed_State *ed, const char *fmt, ...)
{
    // lol
    fputs("?\n", stderr);
    if (ed->help) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

// Append `s` to `b`, transferring ownership to `b`.
internal mem_Allocator_Error
ed_buffer_append(ed_Buffer *b, String s, mem_Allocator allocator)
{
    mem_Allocator_Error err = MEM_OK;
    if (b->len + 1 > b->cap) {
        ed_Line *new_data;
        size_t new_cap;

        new_cap = b->cap * 2;
        if (new_cap == 0) {
            new_cap = 8;
        }

        new_data = mem_resize_array(ed_Line,
            allocator,
            b->data, b->cap,
            new_cap,
            &err);
        
        if (new_data == NULL) {
            return err;
        }
    }
    b->data[b->len++] = (ed_Line){0};
    return err;
}

internal mem_Allocator_Error
ed_open_file(ed_State *ed, const char *file_name)
{
    FILE *file_ptr;
    String_Builder line;
    mem_Allocator_Error err = MEM_OK;

    file_ptr = fopen(file_name, "r");
    if (file_ptr == NULL) {
        ed_error(ed, "Failed to open file '%s'.\n", file_name);
        return MEM_INVALID_ARGUMENT; // I guess?
    }

    string_builder_init_none(&line, ed->allocator);

    // Read the file's lines into memory.
    for (;;) {
        String s;
        char buf[BUFSIZ];
        s.data = fgets(buf, sizeof(buf), file_ptr);
        if (s.data == NULL) {
            break;
        }

        s = string_make_cstring(s.data);
        for (;;) {
            String last, next;
            size_t i = string_index_char(s, '\n');
            if (i == STRING_NOT_FOUND) {
                break;
            }
            last = string_slice_until(s, i);
            next = string_slice_from(s, i + 1);
            err  = string_write_string(&line, last);
            ed_buffer_append(&ed->buffer, last, ed->allocator);
            s = next;
        }
        err = string_write_string(&line, s);
        if (err) {
            string_builder_destroy(&line);
            goto cleanup_file;
        }
    }

cleanup_file:
    fclose(file_ptr);
    return err;
}

internal void
ed_state_init(ed_State *ed, const char *prompt, mem_Allocator allocator)
{
    ed_buffer_init(&ed->buffer);
    ed->allocator = allocator;
    ed->prompt    = prompt;
    ed->help      = true;
}

#define ed_error(ed, fmt, ...)  (ed_error)(ed, fmt "\n", __VA_ARGS__)

internal void
ed_run(ed_State *ed, String line)
{
    size_t i = 0, n = line.len;
    /** @brief 2. Look for the command proper.
     * @link https://pubs.opengroup.org/onlinepubs/9799919799/utilities/ed.html
     */
    switch (line.data[i]) {
    case 'a':
    case 'c':
    case 'd':
    case 'e':
    case 'E':
    case 'f':
    case 'g':
    case 'G':
    case 'h':
    case 'H':
    case 'i':
    case 'j':
    case 'k':
    case 'l':
    case 'm':
    case 'n':
    case 'p':
    case 'P':
    case 'q':
    case 'Q':
    case 'r':
    case 's':
    case 't':
    case 'u':
    case 'v':
    case 'V':
    case 'w':
    case '=':
    case '!':
    default:
        ed_error(ed, "Command '%c' not implemented.", line.data[i]);
        break;
    }
}

internal bool
ed_get_line(ed_State *ed, String_Builder *b, String *line)
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
                    "Command line has maximum of %zu characters.",
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
    String_Builder b;
    char buf[512];
    mem_Arena arena;

    // TODO: Allow `-p` flag from CLI? Since it's always nul-terminated,
    //       we don't need to worry about allocations.
    ed_state_init(&ed, "*", mem_global_heap_allocator());
    mem_arena_init(&arena, buf, sizeof(buf));
    string_builder_init_cap(&b, sizeof(buf), mem_arena_allocator(&arena));
    for (;;) {
        String line;
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
    string_builder_destroy(&b);
    return 0;
}
