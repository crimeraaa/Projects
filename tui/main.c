#include "tui_win.h"
#include <consoleapi2.h>
#include <winbase.h>

#define cast(T)             (T)
#define count_of(array)     (sizeof(array) / sizeof((array)[0]))

static COORD const TOP_LEFT = {0, 0};

typedef struct repl_State repl_State;
struct repl_State {
    tui_Windows T;
    CHAR_INFO * user_buffer;
    COORD       user_cursor;

    /*
     Track many characters are available to be written in the user buffer.
     In other words, this is the maximum x-offset for the user buffer.

     This is necessary to prevent buffer overflows as our grid buffer is
     NOT nul-terminated.
     */
    i16         user_len;


    /*
     Index of the first character in the user buffer not reserved for
     messages. This enables us to preserve prompts when handling
     interactive input.
     */
    i16         user_reserved;
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

    len   = T->grid_size.X;
    stop  = start + len;

    // Loop state.
    s->data = start;
    s->len  = len;
    *state  = stop;
    return true;
}


static void
repl_update_user_cursor(repl_State *R, i16 x)
{
    R->user_cursor.X = x;
    SetConsoleCursorPosition(R->T.handle, R->user_cursor);
}

static void
repl_update_user_buffer(repl_State *R, i16 i, char c)
{
    R->user_buffer[i].Char.AsciiChar = c;
}

/*
 Description:
    Clears the user buffer and moves the cursor back to the left-most
    position.
 */
static void
repl_reset_user_buffer(repl_State *R)
{
    for (i16 i = R->user_reserved, n = R->user_cursor.X; i < n; i++) {
        repl_update_user_buffer(R, i, ' ');
    }
    repl_update_user_cursor(R, R->user_reserved);
}

/*
 Description:
     Mark all indices within the user buffer, up but not including the cursor,
     as immutable.
 */
static void
repl_set_user_reserved(repl_State *R)
{
    R->user_reserved = R->user_cursor.X;
}

static void
repl_write_char(repl_State *R, char c)
{
    // Otherwise, if we'd overflow, don't write anymore.
    int x = R->user_cursor.X + 1;
    if (x <= R->user_len) {
        repl_update_user_buffer(R, x - 1, c);
        repl_update_user_cursor(R, x);
    }
}

static char
repl_pop_char(repl_State *R)
{
    char *p;
    char  c = 0;
    int   x = R->user_cursor.X - 1;
    if (x >= R->user_reserved) {
        p  = &R->user_buffer[x].Char.AsciiChar;
        c  = *p;
        *p = ' ';
    } else {
        x = R->user_reserved;
    }
    repl_update_user_cursor(R, x);
    return c;
}

/*
 Description:
    Writes the given string, of specified length, to the user buffer.
    If the length would overflow the remainder of the buffer, then it
    is clamped. The user buffer x-offset is incremented accordingly.
 */
static void
repl_write_string(repl_State *R, char const *s, i16 n)
{
    i16 x_offset = R->user_cursor.X;
    i16 n_writes = n;

    // Ensure that if we were to write this string starting at the current
    // offset then we don't overflow the buffer. This means we may not write
    // the entire string- we'll report how many bytes we actually wrote.
    if (n_writes > R->user_len - x_offset) {
        // We assume that this will never become negative.
        n_writes = R->user_len - x_offset;
    }

    for (i16 i = 0; i < n_writes; i++) {
        repl_update_user_buffer(R, i + x_offset, s[i]);
    }

    // Could have written 0 bytes (e.g. at end of visible buffer)
    repl_update_user_cursor(R, x_offset + n_writes);
}

static bool
repl_init(repl_State *R, CHAR_INFO *grid,  i16 x, i16 y)
{
    tui_Windows *T    = &R->T;
    COORD const  dims = {x, y};

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

    // if (!SetConsoleScreenBufferSize(T->handle, dims)) {
    //     return false;
    // }

#else
    // Debug only when we need ASAN reports. Otherwise, our console screen
    // buffer will cause terminal to crash upon ASAN trying to load in!
    T->handle = GetStdHandle(STD_OUTPUT_HANDLE);
#endif

    T->grid        = grid;
    T->grid_end    = grid + (cast(u32)y * cast(u32)x);
    T->grid_size   = dims;
    R->user_buffer = NULL;
    R->user_len    = 0;
    R->user_reserved = 0;

    CHAR_INFO *state = grid;
    repl_Line  line;
    while (repl_lines(R, &line, &state)) {}


    // We assume that the last line *is* the user buffer.
    COORD last_loc = {0, y - 1};
    R->user_cursor = last_loc;
    R->user_buffer = line.data;
    R->user_len    = line.len;
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
        /*Right =*/T->grid_size.X,
        /*Bottom=*/T->grid_size.Y};

    WriteConsoleOutputA(T->handle,
        /*lpBuffer      =*/T->grid,
        /*dwBufferSize  =*/T->grid_size,
        /*dwBufferCoord =*/TOP_LEFT,
        /*lpWriteRegion =*/&write_region);
}

static bool
repl_handle_key_event(repl_State *R, KEY_EVENT_RECORD key)
{
    // Can be false if we previously held the key, and now just released it.
    // In any case we don't want to bother with it.
    if (!key.bKeyDown) {
        return true;
    }

    switch (key.wVirtualKeyCode) {
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
repl_read(repl_State *R)
{
    static char const prompt[] = "Input: ";
    tui_Windows *T = &R->T;

    repl_reset_user_buffer(R);
    repl_write_string(R, prompt, sizeof(prompt) - 1);
    repl_set_user_reserved(R);
    repl_draw(R);

    /*
     TODO(2026-08-12):
        Can we figure out a way to NOT use this handle, and instead
        use *OUR* own damn handle?
     */
    HANDLE input_handle = GetStdHandle(STD_INPUT_HANDLE);
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
        }


        for (u32 i = 0; i < n_read; i++) {
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
    Sleep(1000);

    repl_read(R);
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
        BUFR_STR // User buffer mirror.
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
