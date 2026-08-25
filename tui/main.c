#include "tui_win.c"

typedef struct repl_State repl_State;
struct repl_State {
    tui_State T;
    tui_Box   input_bounds;
    FILE *    log_file;
};

// The 'W' business was taken from Casey Muratori. It's actually a virtual
// drive to the repo, i.e. instead of typing "C:\\Users\\...\\Projects\\tui"
// you can just type "W:\\tui".
//
// On Windows CMD, use the `subst` command like so: `subst W: <path>`
#define LOG_FILE_NAME   "W:\\tui\\log.txt"

static bool
repl_init(repl_State *R, tui_Cell *grid, i16 x, i16 y)
{
    tui_State *T = &R->T;
    R->log_file = freopen(LOG_FILE_NAME, "w", stderr);
    if (!R->log_file) {
        tui_log_errorf("Failed to redirect stderr to file '%s'.", TUI_TEXT(LOG_FILE_NAME));
        return false;
    }
    tui_log_infof("Redirected stderr to file '%s'.", TUI_TEXT(LOG_FILE_NAME));
    tui_log_info("REPL is initializing TUI...");
    if (!tui_init(T, grid, x, y)) {
        tui_log_info("...failed to initialize TUI.");
        fclose(R->log_file);
        return false;
    }
    tui_log_info("...initialized TUI.");

    // We assume that the last 3 lines *are* the user buffer,
    // but only first 2 of these lines can actually be written to.
    tui_set_cursor(T, (tui_Point){0, y - 3});
    R->input_bounds = (tui_Box){tui_get_cursor_point(T), {x - 1, y - 2}};
    tui_log_point(R->input_bounds.start, "REPL start");
    tui_log_point(R->input_bounds.stop,  "REPL end");
    return true;
}

static int
repl_write_error(void)
{
    wchar_t buf[256];

    // what the fuck is wrong with you?
    FormatMessageW(
        /*dwFlags      =*/FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        /*lpSource     =*/NULL,
        /*dwMessageId  =*/GetLastError(),
        /*dwLanguageId =*/0,
        /*lpBuffer     =*/buf,
        /*nSize        =*/count_of(buf),
        /*Arguments    =*/NULL);


    tui_log_errorf("%s", buf);
    return 1;
}

static int
repl_run(repl_State *R)
{
    tui_State *T = &R->T;
    static char const msg[] = "[TEST]: input will go here in 1 second?";

    tui_Box bounds = R->input_bounds;
    tui_append_string(T, bounds, msg, sizeof(msg) - 1);

    // Check if cursor manipulation is working correctly
    tui_delete_left_char(T, bounds);
    tui_append_char(T, bounds, '!');
    tui_draw(T);

    // Let it simmer
    Sleep(1000);

    static char const prompt[]   = "Input: ";
    static i16  const prompt_len = sizeof(prompt) - 1;

    // Remove the previous string we wrote.
    tui_delete_all_chars(T, bounds);
    tui_append_string(T, bounds, prompt, prompt_len);

    // Prevent the prompt from being overridden.
    R->input_bounds.start.x += prompt_len;
    bounds.start.x          += prompt_len;
    tui_draw(T);
    tui_read_line(T, bounds);
    tui_destroy(T);

    fclose(R->log_file);
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
    static tui_Cell info[count_of(grid) - 1];
    for (short i = 0; i < count_of(info); i++) {
        tui_Cell *p         = &info[i];
        p->Char.UnicodeChar = grid[i];
        p->Attributes       = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    }

    short const x = count_of(CROW_STR) - 1;
    short const y = count_of(grid) / x;
    if (!repl_init(&R, info, x, y)) {
        return repl_write_error();
    }
    return repl_run(&R);
}
