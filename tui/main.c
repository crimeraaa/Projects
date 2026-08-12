#include "tui_win.h"
#include <consoleapi2.h>

#define cast(T)             (T)
#define count_of(array)     (sizeof(array) / sizeof((array)[0]))

static COORD const TOP_LEFT = {0, 0};

typedef struct repl_State repl_State;
struct repl_State {
    tui_Windows T;
    CHAR_INFO * user_buffer;

    /*
     2-dimensional coordinates of the first valid character we can write to
     in the user buffer. That is, it is beyond the reserved range but before
     the end range.
     */
    COORD       user_cursor;

    /*
     2-dimensional coordinates of the last valid character we can write in
     the user buffer. Use the x-offset to determine the maximum length for
     a line.

     This is necessary to prevent buffer overflows as our grid buffer is
     NOT nul-terminated.
     */
    COORD       user_end;


    /*
     2-dimensional coordinates of the last character in the user buffer
     reserved for messages. This enables us to preserve, say, prompts
     when handling interactive input.

     All coordinates beyond this one are assumed to be valid for mutation by
     the interactive input.
     */
    COORD       user_reserved;
};

typedef struct repl_Line repl_Line;
struct repl_Line {
    i16        len;
    CHAR_INFO *data;
};

static bool
repl_lines(repl_State *R, repl_Line *const s, CHAR_INFO **const state)
{
    tui_Windows *T;
    CHAR_INFO *  start;
    CHAR_INFO *  stop;
    i16          len;

    T     = &R->T;
    start = *state;
    if (start >= T->grid_end) {
        return false;
    }

    len   = R->user_end.X;
    stop  = start + len;

    // Loop state.
    s->data = start;
    s->len  = len;
    *state  = stop;
    return true;
}

#if 0
#include <stdio.h>
#define repl_logf(format, ...) \
    fprintf(stderr, "[INFO] %s:%i: " format "\n", \
        __func__, __LINE__, __VA_ARGS__)
#else
#define repl_logf(format, ...)  ((void)0)
#endif

#define repl_log(message, pos) \
    repl_logf("%s [x = %2i, y = %2i]", message, (pos).X, (pos).Y)

#define repl_logln(message) \
    repl_logf("%s", message)

/*
 Description:
    Sets the current user cursor to the given 2-dimensional coordinates.
    This function assumes you know what you are doing; it does not
    bounds-check!
 */
static void
repl_update_user_cursor(repl_State *R, COORD pos)
{
    R->user_cursor = pos;
    SetConsoleCursorPosition(R->T.handle, pos);
}

/*
 Description:
    Converts the absolute 2-dimensional coordinates into an absolute
    1-dimensional index. This function assumes you know what you are
    doing!
 */
static u32
repl_make_index(repl_State *R, COORD pos)
{
    u32 x, y, k;

    x = cast(u32)pos.X;
    y = cast(u32)pos.Y;
    k = cast(u32)R->T.grid_size.X;
    return x + (y * k);
}

static char *
repl_get_user_buffer(repl_State *R, COORD pos)
{
    u32 i = repl_make_index(R, pos);
    return &R->user_buffer[i].Char.AsciiChar;
}

/*
 Description:
    Unconditionally writes the given character to the TUI grid location,
    specified by the asolute x and y coordinates. This function assumes
    you know what you are doing as it does not bounds-check!
 */
static void
repl_update_user_buffer(repl_State *R, COORD pos, char c)
{
    *repl_get_user_buffer(R, pos) = c;
    repl_logf("Wrote character '%c'", c);
}

static bool
repl_coords(COORD *out, COORD *state, COORD stop)
{
    repl_log("state = ", *state);
    repl_log("stop  = ", stop);
    *out = *state;
    // Innermost loop: column-wise.
    if (state->X + 1 <= stop.X) {
        state->X++;
        return true;
    }

    // Exhausted the columns for this current line, so prepare to move onto
    // the next line.
    state->X = 0;
    out->X   = 0;

    // Outermost loop: line-wise.
    if (state->Y + 1 <= stop.Y) {
        state->Y++;
        return true;
    }

    // We've run out of lines.
    return false;
}

/*
 Description:
    Clears the user buffer and moves the cursor back to the left-most
    position.
 */
static void
repl_reset_user_buffer(repl_State *R)
{
    COORD start = R->user_reserved;
    COORD stop  = R->user_cursor;
    for (COORD pos, state = start; repl_coords(&pos, &state, stop);) {
        repl_update_user_buffer(R, pos, ' ');
    }
    repl_update_user_cursor(R, start);
}

/*
 Description:
     Mark all indices within the user buffer, up but not including the cursor,
     as immutable.
 */
static void
repl_reserve_current(repl_State *R)
{
    R->user_reserved = R->user_cursor;
}

/*
 Description:
    'Increments' the given coordinates, wrapping it in accordance to the
    given stop coordinate.
 */
static bool
repl_coord_incr(COORD *pos, COORD stop)
{
    // Don't add 1; we still want to increment to go *over* so
    // that we can proceed to the last, reserved, line. Subsequent
    // calls, however, won't be able to write into it.
    if (pos->Y <= stop.Y) {
        // We can keep writing to the current line?
        if (pos->X + 1 <= stop.X) {
            pos->X++;
        }
        // We can write to the *next* line, excluding the last one?
        else {
            pos->X = 0;
            pos->Y++;
        }
        return true;
    }
    // We would overflow the buffer otherwise.
    return false;
}


static bool
repl_write_char(repl_State *R, char c)
{
    COORD curr = R->user_cursor;
    COORD next = curr;
    bool  ok   = repl_coord_incr(&next, R->user_end);
    if (ok) {
        repl_update_user_buffer(R, curr, c);
        repl_update_user_cursor(R, next);
    }
    return ok;
}

static bool
repl_coord_decr(COORD *pos, COORD start, COORD stop)
{
    // If we're on the same line as the reserved region, then clamp to
    // their x-offset. Otherwise we're on a line we have full free reign
    // over.
    i16 x_check = (pos->Y == start.Y) ? start.X : 0;
    if (pos->X - 1 >= x_check) {
        pos->X--;
    } else if (pos->Y - 1 >= start.Y) {
        pos->X = stop.X;
        pos->Y--;
    } else {
        return false;
    }
    return true;
}

static char
repl_pop_char(repl_State *R)
{
    char *p;
    char  c    = 0;
    COORD prev = R->user_cursor;
    if (repl_coord_decr(&prev, R->user_reserved, R->user_end)) {
        p  = repl_get_user_buffer(R, prev);
        c  = *p;
        *p = ' ';
        repl_update_user_cursor(R, prev);
    }
    return c;
}

/*
 Description:
    Writes the given string, of specified length, to the user buffer.
    Said string may be split across multiple lines.
 */
static void
repl_write_string(repl_State *R, char const *s, i16 n)
{
    for (i16 i = 0; i < n; i++) {
        repl_logf("Wrote char %i '%c'", i, s[i]);
        if (!repl_write_char(R, s[i])) {
            break;
        }
    }
}

static bool
repl_init(repl_State *R, CHAR_INFO *grid, i16 x, i16 y)
{
    tui_Windows *T    = &R->T;
    COORD const  size = {x, y};
#if 1
    // https://learn.microsoft.com/en-us/windows/console/createconsolescreenbuffer
    T->handle = CreateConsoleScreenBuffer(
        /*dwDesiredAccess     =*/GENERIC_READ | GENERIC_WRITE,
        /*dwShareMode         =*/0,
        /*lpSecurityAttributes=*/NULL,
        /*dwFlags             =*/CONSOLE_TEXTMODE_BUFFER,
        /*lpScreenBufferData  =*/NULL);

    if (T->handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    if (!SetConsoleActiveScreenBuffer(T->handle)) {
        return false;
    }

    // May fail- that's fine. This is just a precaution in case our grid is
    // larger than the current buffer. Otherwise, we'd potentially overrun
    // said internal buffer without realizing.
    SetConsoleScreenBufferSize(T->handle, size);
#else
    // Debug only when we need ASAN reports. Otherwise, our console screen
    // buffer will cause terminal to crash upon ASAN trying to load in!
    T->handle = GetStdHandle(STD_OUTPUT_HANDLE);
#endif

    T->grid          = grid;
    T->grid_end      = grid + (cast(u32)x * cast(u32)y);
    T->grid_size     = size;

    CHAR_INFO *state = grid;
    repl_Line  line;
    while (repl_lines(R, &line, &state)) {}


    // We assume that the last 3 lines *are* the user buffer,
    // but only first 2 of these lines can actually be written to.
    R->user_cursor   = (COORD){0, y - 3};
    R->user_buffer   = line.data;
    R->user_end      = (COORD){x - 1, y - 2};
    R->user_reserved = R->user_cursor;
    return true;
}

static void
repl_draw(repl_State *R)
{
    tui_Windows *T = &R->T;

    // This is necessary so we can 'wrap' the output lines without actually
    // encoding newlines.
    SMALL_RECT write_region = {
        /*Left  =*/0,
        /*Top   =*/0,
        /*Right =*/R->user_end.X,
        /*Bottom=*/R->user_end.Y};

    WriteConsoleOutputA(T->handle,
        /*lpBuffer      =*/T->grid,
        /*dwBufferSize  =*/T->grid_size,
        /*dwBufferCoord =*/TOP_LEFT,
        /*lpWriteRegion =*/&write_region);
}

/*
 TODO(2026-08-13)
    Add arrow key movement through the buffer? This will require a LOT of
    handling...
 */
static bool
repl_handle_key_event(repl_State *R, KEY_EVENT_RECORD key)
{
    // Can be false if we previously held the key, and now just released it.
    // In any case we don't want to bother with it.
    if (!key.bKeyDown) {
        return true;
    }

    switch (key.wVirtualKeyCode) {
    /*
     TODO(2026-08-13):
        Add <Ctrl><Backspace> support to erase entire alphanumeric sequences
        at a time?
     */
    case VK_BACK:
        // Don't literally write the '\b' byte!
        repl_pop_char(R);
        break;
    case VK_RETURN:
    case VK_ESCAPE:
        // Terminate input.
        return false;
    case VK_TAB:
        for (int i = 0; i < 4; i++) {
            repl_write_char(R, ' ');
        }
        break;
    default:
        /*
         Early return for keys that aren't the alphanumerics or their <Shift>
         versions. E.g. '1' and '!' both come from the same physical key on
         U.S. keyboards, but only differ in whether <Shift> was held or not.
         */
        if (   !('0' <= key.wVirtualKeyCode && key.wVirtualKeyCode <= '9')
            && !('A' <= key.wVirtualKeyCode && key.wVirtualKeyCode <= 'Z')) {
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
        repl_write_char(R, key.uChar.AsciiChar);
        break;
    }
    repl_draw(R);
    return true;
}

static int
repl_write_error(void)
{
    char buf[256];
    u32  len, fuck_you;

    // what the fuck is wrong with you?
    len = FormatMessageA(
        /*dwFlags      =*/FORMAT_MESSAGE_FROM_SYSTEM,
        /*lpSource     =*/NULL,
        /*dwMessageId  =*/GetLastError(),
        /*dwLanguageId =*/LANG_USER_DEFAULT,
        /*lpBuffer     =*/buf,
        /*nSize        =*/sizeof(buf),
        /*Arguments    =*/NULL);

    WriteConsoleOutputCharacterA(
        /*hConsoleOutput        =*/GetStdHandle(STD_ERROR_HANDLE),
        /*lpCharacter           =*/buf,
        /*nLength               =*/len,
        /*dwWriteCoord          =*/TOP_LEFT,
        /*lpNumberOfCharsWritten=*/&fuck_you);
    return 1;
}

static void
repl_read(repl_State *R, HANDLE input_handle)
{
    tui_Windows *T = &R->T;

    bool is_reading = true;
    while (is_reading) {
        /*
         TODO(2026-08-12):
            I don't know how much better it is to pass multiple of these
            versus just passing one. Oh well..
         */
        INPUT_RECORD inputs[80];
        u32          n_read = 0;
        if (!ReadConsoleInputA(input_handle, inputs, count_of(inputs), &n_read)) {
            repl_write_error();
            is_reading = false;
            break;
        }

        for (u32 i = 0; i < n_read; i++) {
            // Discard all non-key events.
            if (n_read && inputs[i].EventType != KEY_EVENT) {
                continue;
            }

            is_reading = repl_handle_key_event(R, inputs[i].Event.KeyEvent);
            if (!is_reading) {
                break;
            }
        }
    }
}

static int
repl_run(repl_State *R)
{
    static char const msg[] = "[TEST]: input will go here in 1 second?";
    repl_write_string(R, msg, sizeof(msg) - 1);

    // Check if cursor manipulation is working correctly
    repl_pop_char(R);
    repl_write_char(R, '!');
    repl_draw(R);

    // Let it simmer
    Sleep(1000);

    static char const prompt[] = "Input: ";
    repl_reset_user_buffer(R);
    repl_write_string(R, prompt, sizeof(prompt) - 1);
    repl_reserve_current(R);
    repl_draw(R);

    HANDLE h_input = GetStdHandle(STD_INPUT_HANDLE);
    repl_read(R, h_input);
    return 0;
}

// Row of candidates.
#define CROW_STR "   . . .   . . .   . . .  |  . . .   . . .   . . .  |  . . .   . . .   . . .   "
#define CSEP_STR "                          |                         |                          "
#define BSEP_STR "--------------------------+-------------------------+--------------------------"
#define BUFR_STR "                                                                               "

// Row of boxes.
#define BROW_STR \
    CROW_STR \
    CROW_STR \
    CROW_STR \
    CSEP_STR \
    CROW_STR \
    CROW_STR \
    CROW_STR \
    CSEP_STR \
    CROW_STR \
    CROW_STR \
    CROW_STR

int
main(void)
{
    repl_State R;
    static char const grid[] = {
        BROW_STR
        BSEP_STR
        BROW_STR
        BSEP_STR
        BROW_STR
        BUFR_STR // User buffer mirror line 1.
        BUFR_STR // User buffer mirror line 2.
        BUFR_STR // User buffer mirror line 3- reserved for cursor wrapping only.
    };

    // Sans nul terminators.
    static CHAR_INFO info[count_of(grid) - 1];
    for (short i = 0; i < count_of(info); i++) {
        CHAR_INFO *p      = &info[i];
        p->Char.AsciiChar = grid[i];
        p->Attributes     = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    }

    short const x = count_of(CROW_STR) - 1;
    short const y = count_of(grid) / x;
    if (!repl_init(&R, info, x, y)) {
        return repl_write_error();
    }
    return repl_run(&R);
}
