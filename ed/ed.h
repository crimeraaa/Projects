#ifndef ED_IS_THE_STANDARD_TEXT_EDITOR_H
#define ED_IS_THE_STANDARD_TEXT_EDITOR_H

#include <stdio.h> // FILE

#include "../mem/mem.h"

typedef struct ed_Line ed_Line;
struct ed_Line {
    ed_Line *prev; // Link to the line *before* us.
    ed_Line *next; // Link to the line *after* us.
};


/** @brief A copy of the text of some file. */
typedef struct ed_Buffer ed_Buffer;
struct ed_Buffer {
    ed_Line *tail;
    ed_Line *head;
};

typedef struct ed_State ed_State;
struct ed_State {
    mem_Allocator allocator;
    const char *prompt;
    bool help;
};

global void
ed_init(ed_State *ed, const char *prompt, mem_Allocator allocator);

#if defined __GNUC__ || defined __clang__
__attribute__((__format__(printf, 4, 5)))
#endif
global void
ed_error(ed_State *ed, const char *file, int line, const char *fmt, ...);


#define ed_error(ed, fmt, ...) \
    (ed_error)(ed, __FILE__, __LINE__, fmt "\n", __VA_ARGS__)


/** @brief Read all of a file's lines into the ed buffer. Each line
 *         is a separate, owned allocation.
 *
 * @return The number of lines in the file. -1 indicates an error.
 */
global int
ed_read_file(ed_State *ed, const char *name, size_t *size);

/** @brief Read a newline-terminated string from the given file.
 *
 * @return A pointer to the malloc'd data. The number of occupied
 *         characters is stored in `len`.
 */
global char *
ed_read_stream_line(ed_State *ed, FILE *f, size_t *len);

#endif // !ED_IS_THE_STANDARD_TEXT_EDITOR_H
