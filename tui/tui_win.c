#include "tui_win.h"

#include <stdarg.h>
#include <time.h>

int
(tui_logf)(int level, wchar_t const *path, int line, wchar_t const *format, ...)
{
    va_list        args;
    wchar_t const *file;
    int            n_written = 0;

    // Point to just the file name, ignore directories and such
    file = wcsrchr(path, L'\\');
    if (file) {
        file++;
    } else {
        file = path;
    }

    static wchar_t const LOG_LEVELS[][8] = {
        L"[INFO ]",
        L"[WARN ]",
        L"[ERROR]",
        L"[FATAL]",
        L"[PANIC]",
    };

    time_t     now = time(NULL);
    struct tm *t   = localtime(&now);
    wchar_t    head[64];
    swprintf(head, count_of(head), L"%s:%i ", file, line);
    head[63] = 0;

    n_written += fwprintf(stderr,
        L"%02i:%02i:%02i %s %16s",
        t->tm_hour, t->tm_min, t->tm_sec,
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
tui_init(tui_State *T, CHAR_INFO *grid, i16 x, i16 y)
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

static wchar_t *
tui_resolve_point(tui_State *T, tui_Point pos)
{
    u32 x, y, k;
    x = cast(u32)pos.x;
    y = cast(u32)pos.y;
    k = cast(u32)T->grid_size.x;
    return &T->grid[x + (y * k)].Char.UnicodeChar;
}

wchar_t
tui_peek_at(tui_State *T, tui_Point pos)
{
    return *tui_resolve_point(T, pos);
}

void
tui_poke_at(tui_State *T, tui_Point pos, wchar_t c)
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

wchar_t
tui_remove_prev_char(tui_State *T, tui_Box bounds)
{
    tui_Point prev = T->cursor;
    if (tui_point_decr(&prev, bounds)) {
        wchar_t c = tui_peek_at(T, prev);
        tui_poke_at(T, prev, ' ');

        tui_logf_point(prev, "Removed char '%c'", c);
        tui_set_cursor(T, prev);
        return c;
    }
    return 0;
}

u32
tui_remove_chars(tui_State *T, tui_Box bounds)
{
    u32 n = 0;
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

bool
tui_append_char(tui_State *T, tui_Box bounds, wchar_t c)
{
    tui_Point curr = T->cursor;
    tui_Point next = curr;

    bool ok = tui_point_incr(&next, bounds);
    if (ok) {
        tui_logf_point(curr, "Wrote char '%c'", c);
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
        wchar_t c = cast(wchar_t)s[i];
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
 TODO(2026-08-13)
    Add arrow key movement through the buffer? This will require a LOT of
    handling...
 */
static bool
tui_handle_key_event(tui_State *T, tui_Box bounds, KEY_EVENT_RECORD k, u32 *n)
{
    // Can be false if we previously held the key, and now just released it.
    // In any case we don't want to bother with it.
    if (!k.bKeyDown) {
        return true;
    }

    switch (k.wVirtualKeyCode) {
    /*
     TODO(2026-08-13):
        Add <Ctrl><Backspace> support to erase entire alphanumeric sequences
        at a time?
     */
    case VK_BACK:
        // Don't literally write the '\b' byte!
        if (tui_remove_prev_char(T, bounds)) {
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
    HANDLE h_input    = T->h_input;
    u32    n_written  = 0;
    bool   is_reading = true;
    while (is_reading) {
        /*
         TODO(2026-08-12):
            I don't know how much better it is to pass multiple of these
            versus just passing one. Oh well..
         */
        INPUT_RECORD inputs[80];
        DWORD        n_read = 0;
        if (!ReadConsoleInputW(h_input, inputs, count_of(inputs), &n_read)) {
            is_reading = false;
            break;
        }

        for (DWORD i = 0; i < n_read; i++) {
            // Discard all non-key events.
            if (n_read && inputs[i].EventType != KEY_EVENT) {
                continue;
            }

            is_reading = tui_handle_key_event(T,
                bounds,
                inputs[i].Event.KeyEvent,
                &n_written);

            if (!is_reading) {
                break;
            }
        }
    }
    return n_written;
}
