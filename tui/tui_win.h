#ifndef TUI_WINDOWS_H
#define TUI_WINDOWS_H

/* stfu microslop */
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h> /* BYTE, DWORD, COORD, HANDLE, WORD */
#include <limits.h>  /* CHAR_BIT */
#include <stddef.h>  /* size_t */
#include <stdbool.h> /* bool, false, true */
#include <stdio.h>   /* fprintf, stderr */
#include <stdint.h>  /* [u]int\d+_t */
#include <wchar.h>   /* wchar_t */

#define cast(T)             (T)
#define count_of(array)     (sizeof(array) / sizeof((array)[0]))

typedef uint8_t  byte;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

#if 1

#define LOG_INFO  0
#define LOG_WARN  1
#define LOG_ERROR 2
#define LOG_FATAL 3
#define LOG_PANIC 4

int
(tui_logf)(int level, wchar_t const *path, int line, wchar_t const *format, ...);

#define WIDE2(x)    L ## x
#define WIDE1(x)    WIDE2(x)
#define WFILE       WIDE1(__FILE__)

#define tui_logf2(level, format, ...) \
    (tui_logf)(level, WFILE, __LINE__, L ## format "\n", __VA_ARGS__)

#else
#define tui_logf2(format, ...)  ((void)0)
#endif

#define tui_log_infof(format, ...)  tui_logf2(LOG_INFO,  format, __VA_ARGS__)
#define tui_log_warnf(format, ...)  tui_logf2(LOG_WARN,  format, __VA_ARGS__)
#define tui_log_errorf(format, ...) tui_logf2(LOG_ERROR, format, __VA_ARGS__)
#define tui_log_fatalf(format, ...) tui_logf2(LOG_FATAL, format, __VA_ARGS__)
#define tui_log_panicf(format, ...) tui_logf2(LOG_PANIC, format, __VA_ARGS__)

#define tui_logf_point(pos, format, ...) \
    tui_log_infof("[x = %2i, y = %2i] " format, (pos).x, (pos).y, __VA_ARGS__)

#define tui_log_point(pos, message) tui_logf_point(pos, "%s", L ## message)

#define tui_log_info(message)   tui_log_infof ("%s", L ## message)
#define tui_log_warn(message)   tui_log_warnf ("%s", L ## message)
#define tui_log_error(message)  tui_log_errorf("%s", L ## message)
#define tui_log_fatal(message)  tui_log_fatalf("%s", L ## message)
#define tui_log_panic(message)  tui_log_panicf("%s", L ## message)

/*
 Description:
    Representation of 2-dimensional coordinates, in terms of character cells.
    This may be absolute or relative depending on the usage.

    We assume that most displays we would ever target won't have screens that
    have dimensions that exceed the range of signed 16-bit integers. Will you
    ever need a 60,0000 x 60,000 TUI character cell buffer, for example?
 */
typedef struct tui_Point tui_Point;
struct tui_Point {
    i16 x, y;
};

typedef struct tui_Box tui_Box;
struct tui_Box {
    tui_Point start, stop;
};


typedef struct tui_State tui_State;
struct tui_State {
    HANDLE     h_output;
    HANDLE     h_input;
    HANDLE     h_saved;
    CHAR_INFO *grid;

    /*
     Description: 
        The dimensions of the grid are given by this point, which, by itself
        is not a valid coordinate.

        The x-offset refers to how many columns, while  the y-offset refers
        to how many rows there are in the grid that was given to us.

        Note that the last valid coordinate is given by 'decrementing' this
        by one (1).
     */
    tui_Point grid_size;

    /*
     Description:
        2-dimensional coordinates of the console cursor's current location.
        This is the location the cursor will be drawn at. Note that we don't
        modify it ourselves- it's up to the application to determine how they
        want to manage its position.
     */
    tui_Point cursor;
};

/*
 Description:
    Initializes the given TUI with the given 2-dimensional screen buffer.
    Said buffer is assumed to be row-major, i.e. C-style matrices.
 */
bool
tui_init(tui_State *T, CHAR_INFO *grid, i16 x, i16 y);

bool
tui_destroy(tui_State *T);

tui_Point
tui_get_cursor_point(tui_State *T);

/*
 Description:
    Sets the output cursor's position to the given 2-dimensional coordinates.
    This function assumes you know what you're doing. It does not do any
    bounds-checking for you!

 Returns:
    `true` if we successfully updated the position else `false`. Although we
    don't check the bounds, we may delegate to OS-level APIs which in turn
    return a status.
 */
bool
tui_set_cursor(tui_State *T, tui_Point pos);


/*
 Description:
    Low-level function which retrieves the character value of the grid cell
    at the specified coordinates. No bounds-checking is performed.

 Returns:
    The character at the specified coordinates.
 */
wchar_t
tui_peek_at(tui_State *T, tui_Point pos);


/*
 Description:
    Low-level function that unconditionally writes the given character to the
    specified coordinates. No bounds-checking is performed nor is the
    console cursor's position updated.
 */
void
tui_poke_at(tui_State *T, tui_Point pos, wchar_t c);


/*
 Description:
    Draws our buffer to the output handle. That's it.

 Returns:
    `true` if no error occured, else `false`. What more could you want?
 */
bool
tui_draw(tui_State *T);

/*
 Description:
    Writes the given character to the current cursor position and moves the
    cursor accordingly if the adjusted cursor would still be in range of
    the given bounds.

 Returns:
    `true` if the append was performed successfully else `false` if we had
    no more space in the buffer to do so.
 */
bool
tui_append_char(tui_State *T, tui_Box bounds, wchar_t c);

/*
 Description:
    Writes the given ANSI string, of specified length, to the current
    cursor position and moves the cursor accordingly if the adjusted
    cursor would still be in range of the given bounds.

    Note that the string may be split across multiple lines.

 Returns:
    The actual number of characters appended. This is always at least equal
    to the given length, but it could also be less in the event we run out
    of space in the buffer.
 */
u32
tui_append_string(tui_State *T, tui_Box bounds, char const *s, i16 n);

/*
 Description:
    Deletes the character at the cursor's current position and moves it back
    by one (1) cell, wrapping it in accordance to the given bounds.

 Returns:
    The character value that was removed, else 0.
 */
wchar_t
tui_remove_prev_char(tui_State *T, tui_Box bounds);

/*
 Description:
    Deletes all characters in the given bounds and moves the cursor back to
    the start of said bounds.

 Returns:
    The number of characters successfully deleted.
 */
u32
tui_remove_chars(tui_State *T, tui_Box bounds);

/*
 Description:
    Interactive input. Reads a line, or multiple lines, of text from the user.
    The given bounds form a region where text input from the user will be
    mirrored on the TUI output.

    The bounds are also used to limit how many characters can be entered by
    the user- any more inputs get discarded until the user presses <Backspace>
    to delete 1 or more characters, or hits <Enter> to terminate input.

 Returns:
    The numbers of user-inputted characters stored in the region at the time
    <Enter> was pressed.

 TODO(2026-08-13):
    Take in a user-supplied character buffer?
    Determine if we want to save newlines or not?
    Allow cursor movement?
 */
u32
tui_read_line(tui_State *T, tui_Box bounds);

#endif // !TUI_WINDOWS_H
