#include "tui.h"
#include <stdarg.h>
#include <time.h>

int
(tui_logf)(tui_LogLevel level, tui_char const *path, int line, tui_char const *format, ...)
{
    va_list        args;
    tui_char const *file;
    int            n_written = 0;

    // Point to just the file name, ignore directories and such
    file = wcsrchr(path, L'\\');
    if (file) {
        file++;
    } else {
        file = path;
    }

    static tui_char const LOG_LEVELS[][8] = {
        L"[INFO ]",
        L"[WARN ]",
        L"[ERROR]",
        L"[FATAL]",
        L"[PANIC]",
    };

    time_t    now  = time(NULL);
    struct tm tnow = *localtime(&now);
    tui_char  head[64];
    swprintf(head, count_of(head), L"%s:%i ", file, line);
    head[63] = 0;

    n_written += fwprintf(stderr,
        L"%02i:%02i:%02i %s %16s",
        tnow.tm_hour, tnow.tm_min, tnow.tm_sec,
        LOG_LEVELS[level],
        head);

    va_start(args, format);
    n_written += vfwprintf(stderr, format, args);
    va_end(args);
    return n_written;
}

/*
 Description:
    Glue code because I am *NOT* using `COORD` lil bro
 */
static COORD
tui_point_to_coord(tui_Point pos)
{
    return (COORD){pos.x, pos.y};
}

bool
tui_init(tui_State *T, tui_Cell *grid, i16 x, i16 y)
{
    tui_Point size = {x, y};
    tui_log_point(size, "Received TUI grid size");

    T->h_input = GetStdHandle(STD_INPUT_HANDLE);
    if (T->h_input == INVALID_HANDLE_VALUE) {
        tui_log_error("Failed to get a handle to the standard input!");
        return false;
    }

    // Save the current screen console output buffer so that we can restore
    // it upon exit. This, of course, assumes the buffer *is* stdout...
    T->h_saved = GetStdHandle(STD_OUTPUT_HANDLE);
    if (T->h_saved == INVALID_HANDLE_VALUE) {
        tui_log_error("Failed to save a handle to the current console screen buffer!");
        return false;
    }

#if 1
    // https://learn.microsoft.com/en-us/windows/console/createconsolescreenbuffer
    T->h_output = CreateConsoleScreenBuffer(
        /*dwDesiredAccess     =*/GENERIC_READ | GENERIC_WRITE,
        /*dwShareMode         =*/0,
        /*lpSecurityAttributes=*/NULL,
        /*dwFlags             =*/CONSOLE_TEXTMODE_BUFFER,
        /*lpScreenBufferData  =*/NULL);

    if (T->h_output == INVALID_HANDLE_VALUE) {
        tui_log_error("Failed to create new console screen buffer!");
        return false;
    }

    if (!SetConsoleActiveScreenBuffer(T->h_output)) {
        CloseHandle(T->h_output);
        tui_log_error("Failed to set the handle of the console's active screen buffer!");
        return false;
    }

    // May fail if the given size is smaller than the current one- that's ok.
    // This is just a precaution in case our grid is larger than the current
    // buffer. Otherwise, we'd potentially overrun said internal buffer
    // without realizing.
    SetConsoleScreenBufferSize(T->h_output, tui_point_to_coord(size));
#else
    // Debug only when we need ASAN reports. Otherwise, our console screen
    // buffer will cause terminal to crash upon ASAN trying to load in!
    T->handle = GetStdHandle(STD_OUTPUT_HANDLE);
#endif

    T->grid      = grid;
    T->grid_size = size;

    // Console cursor in the buffer is already at the top left so we don't
    // need to make the API call.
    T->cursor = (tui_Point){0, 0};
    return true;
}

bool
tui_destroy(tui_State *T)
{
    if (SetConsoleActiveScreenBuffer(T->h_saved)) {
        if (CloseHandle(T->h_output)) {
            return true;
        }
    }
    return false;
}

tui_Point
tui_get_cursor_point(tui_State *T)
{
    return T->cursor;
}

bool
tui_set_cursor(tui_State *T, tui_Point pos)
{
    T->cursor = pos;
    // tui_log_point(pos, "Updated TUI cursor");
    return SetConsoleCursorPosition(T->h_output, tui_point_to_coord(pos));
}

static tui_char *
tui_resolve_point(tui_State *T, tui_Point pos)
{
    u32 x, y, k;
    x = cast(u32)pos.x;
    y = cast(u32)pos.y;
    k = cast(u32)T->grid_size.x;

    // In a row-major represntation, the x-offset is the more-frequently
    // changing one. This is a little more cache-friendly as the processor
    // can read multiple elements from a particular row address and access
    // sequential columns from there.
    return &T->grid[x + (y * k)].Char.UnicodeChar;
}

tui_char
tui_peek_at(tui_State *T, tui_Point pos)
{
    return *tui_resolve_point(T, pos);
}

void
tui_poke_at(tui_State *T, tui_Point pos, tui_char c)
{
    *tui_resolve_point(T, pos) = c;
}

bool
tui_draw(tui_State *T)
{
    tui_Point size = T->grid_size;
    // tui_log_point(size, "Drew TUI dimensions");

    // These coordinates are inclusive- the last valid coordinate is off-by-one
    // from the size.
    SMALL_RECT write_region = {
        /*Left  =*/0,          /*Top    =*/0,
        /*Right =*/size.x - 1, /*Bottom =*/size.y - 1
    };

    return WriteConsoleOutputW(T->h_output,
        /*lpBuffer      =*/T->grid,
        /*dwBufferSize  =*/tui_point_to_coord(size),
        /*dwBufferCoord =*/(COORD){0, 0},
        /*lpWriteRegion =*/&write_region);
}


static bool
tui_point_eq(tui_Point a, tui_Point b)
{
    return (a.x == b.x) && (a.y == b.y);
}

/*
 Description:
    'Decrements' the given coordinates, wrapping it in accordance to the
    given bounds.
 */
static bool
tui_point_decr(tui_Point *pos, tui_Box bounds)
{
    // If we're on the same line as the reserved region, then clamp to
    // their x-offset. Otherwise we're on a line we have full free reign
    // over.
    i16 x_start = (pos->y == bounds.start.y) ? bounds.start.x : 0;

    // Can we keep going backward in this line?
    if (pos->x - 1 >= x_start) {
        pos->x--;
        return true;
    }

    // Can we wrap around to the previous line?
    if (pos->y - 1 >= bounds.start.y) {
        pos->x = bounds.stop.x;
        pos->y--;
        return true;
    }
    return false;
}

/*
 Description:
    'Increments' the given coordinates, wrapping it in accordance to the
    given bounds.
 */
static bool
tui_point_incr(tui_Point *pos, tui_Box bounds)
{
    // We can keep writing to the current line?
    if (pos->x + 1 <= bounds.stop.x) {
        pos->x++;
        return true;
    }

    /*
     NOTE(2026-08-13):
        If we add 1, then the cursor remains at the last valid coordinate
        and no character is written there. If we don't add 1, the character
        gets written and the cursor wraps to the next line.
     */
    if (pos->y + 1 <= bounds.stop.y) {
        pos->x = 0;
        pos->y++;
        return true;
    }

    // We would overflow the buffer otherwise.
    return false;
}

/*
 Shifts all cells to the right of the given point by 1 cell to the left.
 */
static void
tui_shift_left(tui_State *T, tui_Point pos, tui_Box bounds)
{
    // Iterate from left to right.
    tui_Point curr = pos;
    for (tui_Point next = curr; !tui_point_eq(curr, bounds.stop); curr = next) {
        // We assume this will never fail.
        tui_point_incr(&next, bounds);
        tui_char c = tui_peek_at(T, next);
        tui_poke_at(T, curr, c);
    }

    // No matter what, when deleting a character, the last valid input position
    // is going to be erased.
    tui_poke_at(T, bounds.stop, ' ');
}

tui_char
tui_delete_left_char(tui_State *T, tui_Box bounds)
{
    tui_Point prev = T->cursor;
    if (tui_point_decr(&prev, bounds)) {
        tui_char c = tui_peek_at(T, prev);
        tui_shift_left(T, prev, bounds);
        tui_logf_point(prev, "Removed char '%c'", c);
        tui_set_cursor(T, prev);
        return c;
    }
    return 0;
}

u32
tui_delete_all_chars(tui_State *T, tui_Box bounds)
{
    u32 n = 0;
    // It's important to iterate row-column (y-x) wise as `x` is our more
    // frequently changing index, so we can make better use of the cache
    // for each `y` iteration.
    for (tui_Point pos = bounds.start; pos.y <= bounds.stop.y; pos.y++) {
        for (; pos.x <= bounds.stop.x; pos.x++) {
            tui_poke_at(T, pos, L' ');
            n++;
        }
        pos.x = 0;
    }
    tui_set_cursor(T, bounds.start);
    return n;
}

/*
 Shifts all cells to the right of the given position by 1 cell to the right.
 */
static void
tui_shift_right(tui_State *T, tui_Point pos, tui_Box bounds)
{
    tui_Point curr = bounds.stop;
    for (tui_Point prev = curr; !tui_point_eq(prev, pos); curr = prev) {
        tui_point_decr(&prev, bounds);

        tui_char c = tui_peek_at(T, prev);
        tui_poke_at(T, curr, c);
    }
}

bool
tui_append_char(tui_State *T, tui_Box bounds, tui_char c)
{
    tui_Point curr = T->cursor;
    tui_Point next = curr;

    // Ensure that the cursor could be incremented and that the last input
    // buffer cell can be overwritten.
    bool ok = tui_point_incr(&next, bounds) && tui_peek_at(T, bounds.stop) == ' ';
    if (ok) {
        tui_logf_point(curr, "Wrote char '%c'", c);
        tui_shift_right(T, curr, bounds);
        tui_poke_at(T, curr, c);
        tui_set_cursor(T, next);
    } else {
        tui_logf_point(curr, "Truncated char '%c'", c);
    }
    return ok;
}

u32
tui_append_string(tui_State *T, tui_Box bounds, char const *s, i16 n)
{
    u32 n_written = 0;
    for (i16 i = 0; i < n; i++, n_written++) {
        tui_char c = cast(tui_char)s[i];
        if (!tui_append_char(T, bounds, c)) {
            break;
        }
    }
    return n_written;
}


static void
tui_tab(tui_State *T, tui_Box bounds)
{
    for (int i = 0; i < 4; i++) {
        tui_append_char(T, bounds, ' ');
    }
}

/*
 Delete an entire WORD (a sequence of non-whitespaces) to the left of the
 cursor. The cursor is placed *after* the first whitespace, or the start
 of the bounds.
 */
static void
tui_delete_left_word(tui_State *T, tui_Box bounds, u32 *n)
{
    for (;;) {
        tui_char c = tui_delete_left_char(T, bounds);
        // Already at the start of the input buffer?
        if (!c) {
            break;
        }
        // Found a WORD separator? We assume that we encode whitespaces.
        // We don't ever encode tabs, line feeds, or carriage returns.
        else if (c == ' ') {
            tui_append_char(T, bounds, c);
            break;
        }

        *n -= 1;
    }
}

static bool
tui_handle_key_event(tui_State *T, tui_Box bounds, KEY_EVENT_RECORD k, u32 *n)
{
    // Can be false if we previously held the key, and now just released it.
    // In any case we don't want to bother with it.
    if (!k.bKeyDown) {
        return true;
    }

    switch (k.wVirtualKeyCode) {
    case VK_LEFT:
        tui_point_decr(&T->cursor, bounds);
        tui_set_cursor(T, T->cursor);
        return true;
    case VK_RIGHT:
        tui_point_incr(&T->cursor, bounds);
        tui_set_cursor(T, T->cursor);
        return true;
    case VK_BACK:
        if (k.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) {
            tui_delete_left_word(T, bounds, n);
        } else if (tui_delete_left_char(T, bounds)) {
            *n -= 1;
        }
        break;
    case VK_RETURN:
    case VK_ESCAPE: // Terminate input.
        return false;
    case VK_TAB:
        tui_tab(T, bounds);
        break;
    default:
        /*
         Early return for keys that aren't the alphanumerics or their <Shift>
         versions. E.g. '1' and '!' both come from the same physical key on
         U.S. keyboards, but only differ in whether <Shift> was held or not.
         */
        if (   !('0' <= k.wVirtualKeyCode && k.wVirtualKeyCode <= '9')
            && !('A' <= k.wVirtualKeyCode && k.wVirtualKeyCode <= 'Z')) {
            return true;
        }

        // fallthrough
    case VK_SPACE:
    case VK_OEM_1:      // ';' and ':' for US keyboards
    case VK_OEM_PLUS:   // '+' for any country
    case VK_OEM_COMMA:  // ',' for any country
    case VK_OEM_MINUS:  // '-' for any country
    case VK_OEM_PERIOD: // '.' for any country
    case VK_OEM_2:      // '/' and '?' for US keyboards
    case VK_OEM_3:      // '`' and '~' for US keyboards
    case VK_OEM_4:
    case VK_OEM_5:
    case VK_OEM_6:
    case VK_OEM_7:
    case VK_OEM_8:
    case VK_OEM_102:
        // Mirror the input character into the user buffer.
        if (tui_append_char(T, bounds, k.uChar.UnicodeChar)) {
            *n += 1;
        }
        break;
    }
    return tui_draw(T);
}

u32
tui_read_line(tui_State *T, tui_Box bounds)
{
    tui_Handle h_input    = T->h_input;
    u32        n_written  = 0;
    bool       is_reading = true;
    while (is_reading) {
        // Based on my testing, we don't need more than 1 input record. It
        // seems the requirement for multiple input records is meant for when
        // you explicitly write the inputs to some handle.
        INPUT_RECORD input;

        // Dummy because it's a required out-parameter. The call, however,
        // will only return when at least 1 input of any ind has been read.
        DWORD n_read = 0;
        if (!ReadConsoleInputW(h_input, &input, 1, &n_read)) {
            is_reading = false;
            break;
        }

        // Discard all non-key events.
        if (input.EventType != KEY_EVENT) {
            tui_log_infof("Got event type %u", input.EventType);
            continue;
        }

        if (!tui_handle_key_event(T, bounds, input.Event.KeyEvent, &n_written)) {
            is_reading = false;
            break;
        }
    }
    return n_written;
}
