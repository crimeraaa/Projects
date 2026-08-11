#include <stdio.h>  // fgets, fputc, fputs, fprintf

#include "sudoku.h"
#include "sudoku.c"

#define cast(T) (T)

/*
 Description:
    This is the template character in the default string representation that
    is to be replaced with the actual value of each digit. It should not appear
    in other contexts within the said representation.
 */
#define REPR_GRID_CHAR   'x'

/*
Refer to: https://en.wikipedia.org/wiki/Box-drawing_characters

             0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
    U+2500   ─   ━ 	 │	 ┃ 	 ┄	 ┅	 ┆	 ┇	 ┈	 ┉	 ┊	 ┋	 ┌	 ┍	 ┎	 ┏ 
    U+251x	 ┐	 ┑	 ┒	 ┓	 └	 ┕	 ┖	 ┗	 ┘	 ┙	 ┚	 ┛	 ├	 ┝	 ┞	 ┟
    U+252x	 ┠	 ┡	 ┢	 ┣	 ┤	 ┥	 ┦	 ┧	 ┨	 ┩	 ┪	 ┫	 ┬	 ┭	 ┮	 ┯
    U+253x	 ┰	 ┱	 ┲	 ┳	 ┴	 ┵	 ┶	 ┷	 ┸	 ┹	 ┺	 ┻	 ┼	 ┽	 ┾	 ┿
    U+254x	 ╀	 ╁	 ╂	 ╃	 ╄	 ╅	 ╆	 ╇	 ╈	 ╉	 ╊	 ╋	 ╌	 ╍	 ╎	 ╏
    U+255x	 ═	 ║	 ╒	 ╓	 ╔	 ╕	 ╖	 ╗	 ╘	 ╙	 ╚	 ╛	 ╜	 ╝	 ╞	 ╟
    U+256x	 ╠	 ╡	 ╢	 ╣	 ╤	 ╥	 ╦	 ╧	 ╨	 ╩	 ╪	 ╫	 ╬	 ╭	 ╮	 ╯
    U+257x	 ╰	 ╱	 ╲	 ╳	 ╴	 ╵	 ╶	 ╷	 ╸	 ╹	 ╺	 ╻	 ╼	 ╽	 ╾	 ╿
 */
#define REPR                      \
    "┌───┬───┬───┰───┬───┬───┰───┬───┬───┐\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "┝━━━┿━━━┿━━━╋━━━┿━━━┿━━━╋━━━┿━━━┿━━━┥\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "┝━━━┿━━━┿━━━╋━━━┿━━━┿━━━╋━━━┿━━━┿━━━┥\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "├───┼───┼───╂───┼───┼───╂───┼───┼───┤\n" \
    "│ x │ x │ x ┃ x │ x │ x ┃ x │ x │ x │\n" \
    "└───┴───┴───┸───┴───┴───┸───┴───┴───┘\n" \


#define REPR_ASCII_ROW_SEP \
    "                         |                           |                         \n"

#define REPR_ASCII_BOX_SEP \
    "-------------------------+---------------------------+-------------------------\n"

/*
   0  1  2   3  4  5   6  7  8 |  9 10 11  12 13 14  15 16 17  |  18 19 20  21 22 23  24 25 26
  27 28 29  30 31 32  33 34 35 | 36 37 38  39 40 41  42 43 44  |  45 46 47  48 49 50  51 52 53
  54 55 56  57 58 59  60 61 62 | 63 64 65  66 67 68  69 70 71  |  72 73 74  75 76 77  78 79 80


   0  1  2   9 10 11  18 19 20 | 27 28 29  36 37 38  45 46 47  |  54 55 56
   3  4  5  12 13 14  21 22 23 | 30 31 32  39 40 41  48 49 50  |  57 58 59
   6  7  8  15 16 17  24 25 26 | 33 34 35  42 43 44  51 52 53  |  60 61 62
 */
#define REPR_ASCII_ROW \
    " x x x   x x x   x x x   |   x x x   x x x   x x x   |   x x x   x x x   x x x \n" \
    " x x x   x x x   x x x   |   x x x   x x x   x x x   |   x x x   x x x   x x x \n" \
    " x x x   x x x   x x x   |   x x x   x x x   x x x   |   x x x   x x x   x x x \n"

#define REPR_ASCII_BOX_ROW \
    REPR_ASCII_ROW         \
    REPR_ASCII_ROW_SEP     \
    REPR_ASCII_ROW         \
    REPR_ASCII_ROW_SEP     \
    REPR_ASCII_ROW         \

#define REPR_ASCII         \
    REPR_ASCII_BOX_ROW     \
    REPR_ASCII_BOX_SEP     \
    REPR_ASCII_BOX_ROW     \
    REPR_ASCII_BOX_SEP     \
    REPR_ASCII_BOX_ROW     \


typedef enum {
    CMD_NONE,
    CMD_QUIT,
    CMD_NEXT,
    CMD_FINISH,
} repl_Cmd;

typedef struct repl_State repl_State;
struct repl_State {
    FILE *   input;
    FILE *   output;
    repl_Cmd prev_cmd;
    int      step_count;

    // Representation information.
    int      line_count;
    char *   grid_buffer;
    size_t   grid_len;

    /*
     Each row and column maps to a box of candidates.
     */
    short    grid_indexes[SUDOKU_GRID_ROWS][SUDOKU_GRID_COLS][SUDOKU_BOX_SIZE];
};

static char const sample_easy[] =
    ". . 5 | . 1 . | . 9 8"
    "6 . 9 | . . . | . . ."
    ". 4 . | 2 . . | . 5 6"
    "------+-------+------"
    "2 8 6 | . 4 . | 5 3 ."
    "5 . . | 3 9 . | . . ."
    ". . . | . 5 2 | . . ."
    "------+-------+------"
    "3 6 1 | 4 . 8 | 9 . ."
    "8 . . | 5 7 3 | . 6 4"
    "4 . . | . . . | . 2 ."
;

static char const sample_hard[] =
    ". 6 . | 8 . . | . 5 ."
    ". . . | 3 . 2 | 9 . ."
    "8 . 3 | 4 . 6 | . . ."
    "------+-------+------"
    ". . 5 | . . . | 6 . ."
    "1 . . | . . . | . 2 4"
    "3 2 . | . . . | . 8 ."
    "------+-------+------"
    ". 1 . | . . 3 | . . ."
    "4 . . | 2 . 7 | . . ."
    ". 7 . | . . . | . . ."
;

static char const sample_vicious[] =
    "4 7 . | . . 5 | . . ."
    "5 . 1 | . 7 . | 2 . ."
    ". 2 . | . . . | . . 1"
    "------+-------+------"
    ". . . | . . . | 9 7 ."
    "1 . . | . . 3 | 5 . ."
    ". 5 . | . 4 . | 6 . ."
    "------+-------+------"
    ". . . | 6 5 7 | . . ."
    ". 6 5 | 3 . . | . . ."
    "3 . . | . 2 . | . . ."
;

// NOTE(2026-08-09): These only work on the *visible* view port.
// https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
#define ESC "\x1b"
#define CSI ESC "["

static void
cursor_previous_line(FILE *stream, int count)
{
    fprintf(stream, CSI "%iF", count);
}

/*
 Arguments
    row - 1-based index. 1 refers to the upper-most row.
    col - 1-based index. 1 refers to the left-most column.
 */
static void
cursor_position(FILE *stream, int row, int col)
{
    fprintf(stream, CSI "%i;%iH", row, col);
}

// Options common to `erase_*()`.
typedef enum {
    ERASE_END,
    ERASE_BEGIN,
    ERASE_ENTIRE,
} erase_Mode;

/*
 Arguments for `mode`:
    ERASE_END    - Clear from cursor to the *end* of the line.
    ERASE_BEGIN  - Clear from cursor to the *beginning* of the line.
    ERASE_ENTIRE - Clear the *entire* line.
 */
static void
erase_line(FILE *stream, erase_Mode mode)
{
    fprintf(stream, CSI "%iK", mode);
}

/*
 Arguments for `mode`:
    ERASE_END    - Clear from cursor to *end* of the screen.
    ERASE_BEGIN  - Clear from cursor to the *beginning* of the screen.
    ERASE_ENTIRE - Clear the *entire* screen. On DOS ANSI.SYS, this also moves
                   the cursor to the upper left.
 */
static void
erase_display(FILE *stream, erase_Mode mode)
{
    fprintf(stream, CSI "%iJ", mode);
}

static void
erase_previous_lines(FILE *stream, int count)
{
    while (count-- > 0) {
        cursor_previous_line(stream, 1);
        erase_line(stream, ERASE_ENTIRE);
    }
}

static char const *
read_line(FILE *stream, size_t *input_len)
{
    static char buf[256];
    char *      input;
    size_t      n = 0;

    input = fgets(buf, sizeof(buf), stream);
    if (input) {
        n        = strcspn(input, "\r\n");
        input[n] = 0;
    }

    if (input_len) {
        *input_len = n;
    }
    return input;
}

static repl_Cmd
read_command(repl_State *R)
{
    char const *input;
    size_t      input_len;
    repl_Cmd    cmd = CMD_NONE;

    for (;;) {
        fprintf(R->output, "Option ([Ff]inish, [Nn]ext, [Qq]uit): ");
        input = read_line(R->input, &input_len);
        if (input_len == 1) switch (input[0]) {
        case 'F':
        case 'f': cmd = CMD_FINISH; break;
        case 'N':
        case 'n': cmd = CMD_NEXT;   break;
        case 'Q':
        case 'q': cmd = CMD_QUIT;   break;
        }

        // Null input indicates EOF. We want to exit no matter what.
        if (!input) {
            cmd = CMD_QUIT;
        }

        if (cmd) {
            R->prev_cmd = cmd;
            return cmd;
        }

        // Non-null input that is just an empty string, along with
        // a previously saved option, indicates repeat said option.
        if (input_len == 0 && R->prev_cmd) {
            return R->prev_cmd;
        }

        // Otherwise, said non-null input is an invalid option.
        // Since the stdout cursor got placed on a new line,
        // move up one line in order to erase the prompt and input.
        erase_previous_lines(R->output, 1);

        // Non-empty (i.e. length is non-zero) input means an invalid option.
        // It's possible to have non-null but empty (i.e. length is zero) input.
        //
        // E.g. the user just typed <Enter>, stdin only contains the newline.
        // This gets trimmed so we only see an empty string. In that case we
        // just want to re-prompt without mentioning the error.
        if (input_len != 0) {
            fprintf(R->output, "Unknown option '%s'. ", input);
        }
    }
    return 0;
}

static void
repl_erase(sudoku_Game *G, repl_State *R, int extra_line_count)
{
    // Erase, for example, step counter information and/or prompt with input.
    // They are variably sized so this helps erase old data we may
    // not overwrite on subsequent calls.
    erase_previous_lines(R->output, R->line_count + extra_line_count);

    // Move the cursor back to the upper left of the grid so we can
    // overwrite it. Since we assume the grid is always the same size
    // across calls, we don't need to erase it since we'll always
    // successfully write on top of old data.
    // cursor_previous_line(R->output, R->line_count);
}

static char
sudoku_digit2char(sudoku_Digit digit)
{
    switch (digit) {
    case 0: return '?';
    case 1: return '1';
    case 2: return '2';
    case 3: return '3';
    case 4: return '4';
    case 5: return '5';
    case 6: return '6';
    case 7: return '7';
    case 8: return '8';
    case 9: return '9';
    }
    return '?';
}

static void
repl_write_digit(repl_State *R, int row, int col, sudoku_Digit digit)
{
    short i = R->grid_indexes[row][col][digit - 1];
    char *p = &R->grid_buffer[i];
    *p = sudoku_digit2char(digit);
}

static char const *
repl_to_string(sudoku_Game *G, repl_State *R)
{
    for (int row = 0; row < SUDOKU_GRID_ROWS; row++) {
        for (int col = 0; col < SUDOKU_GRID_COLS; col++) {
            sudoku_DigitSet set;
            sudoku_Digit    d;

            set = sudoku_candidates(G, row, col);

            // If no candidates, then it must be filled in already.
            if (!set) {
                d  = sudoku_get(G, row, col);
                if (d) {
                    repl_write_digit(R, row, col, d);
                }
                continue;
            }

            for (d = SUDOKU_DIGIT_MIN; d <= SUDOKU_DIGIT_MAX; d++) {
                if (set & (1 << (cast(sudoku_DigitSet)d - 1))) {
                    repl_write_digit(R, row, col, d);
                }
            }
        }
    }
    return R->grid_buffer;
}

static bool
repl_step(sudoku_Game *G, void *user_data, int row, int col)
{
    repl_State *R;
    char const *repr = NULL;
    repl_Cmd    cmd;

    R    = cast(repl_State *)user_data;
    if (R->prev_cmd == CMD_FINISH) {
        return 1;
    }

    cmd = read_command(R);
    switch (cmd) {
    case CMD_NONE:
    case CMD_QUIT:   return false;
    case CMD_NEXT:
    case CMD_FINISH: break;
    }

    if (R->step_count++ > 0) {
        // Erase the previous grid, step counter, candidate info.
        // Also erase the prompt/input we just got.
        repl_erase(G, R, 4);
    } else {
        // Erase the previous grid along with the prompt/input we just got.
        repl_erase(G, R, 2);
    }

    repr = repl_to_string(G, R);
    fprintf(R->output, "%sSteps taken so far: %i.\n", repr, R->step_count);

    sudoku_DigitSet cand_set     = sudoku_candidates(G, row, col);
    int             cand_written = 0;
    fprintf(R->output, "[%i,%i] = 0x%02x{", row, col, cand_set);
    for (sudoku_Digit d = SUDOKU_DIGIT_MIN; d <= SUDOKU_DIGIT_MAX; d++) {
        if (cand_set & (1 << cast(sudoku_DigitSet)(d - 1))) {
            if (cand_written++ > 0) {
                fputc(',', R->output);
            }
            fputc(sudoku_digit2char(d), R->output);
        }
    }
    fputs("}\n", R->output);
    return true;
}

static int
repl_run(sudoku_Game *G, repl_State *R)
{
    char const *repr;
    int         status     = 0;

    repr   = repl_to_string(G, R);
    fprintf(R->output, "%s", repr);
    status = sudoku_solve_stepwise(G, repl_step, R);
    switch (status) {
    case SUDOKU_OK:
        fprintf(R->output, "Steps taken: %i.\n", R->step_count);
        break;
    case SUDOKU_UNSOLVABLE:
        fprintf(R->output, "No solution found after %i steps.\n",
            R->step_count);
        break;
    case SUDOKU_TERMINATED:
        break;
    }
    return 0;
}

static bool
repl_lines(char **out_start, char **out_stop, char **state)
{
    char *p = *state;
    char *q = p;
    for (; *q != 0; q++) {
        if (*q == '\n') {
            break;
        }
    }

    if (*q == 0) {
        return false;
    }

    *out_start = p;
    *out_stop  = q;
    *state     = q + 1;
    return true;
}

static bool
repl_init(repl_State *R, FILE *input, FILE *output, char *buffer, size_t len)
{
    if (!(buffer && len > SUDOKU_GRID_SIZE && buffer[len - 1] == 0)) {
        return false;
    }

    R->input           = input;
    R->output          = output;
    R->prev_cmd        = CMD_NONE;
    R->step_count      = 0;
    R->grid_buffer     = buffer;
    R->grid_len        = len;
    R->line_count = 0;

    // Index of the grid cell we are at.
    int grid_row = 0;

    // Index of the candiate cell, within the current grid cell, we are at.
    int cand_row = 0;


    // This makes us slower as we now traverse (line-count * (line-length * 2))
    // times through the format buffer, but it makes line handling easier.
    char *state = buffer;
    for (char *start, *stop; repl_lines(&start, &stop, &state); R->line_count++) {
        int grid_col = 0;
        int cand_col = 0;
        for (char *p = start; p < stop; p++) {
            short buf_index, cand_index;
            if (*p != REPR_GRID_CHAR) {
                continue;
            }

            buf_index  = cast(short)(p - buffer);
            cand_index = (cand_row * SUDOKU_BOX_ROWS) + cand_col;

            R->grid_indexes[grid_row][grid_col][cand_index] = buf_index;
            buffer[buf_index] = '.';

            // Exhausted the candidate columns for this cell?
            // If so, move on to the next grid column.
            if (++cand_col >= SUDOKU_BOX_COLS) {
                // It's valid to have the number of rows as the current
                // count, as that should mark the end.
                //
                // If our counter goes past the number of rows then
                // we run the risk of a buffer overflow.
                if (++grid_col > SUDOKU_GRID_ROWS) {
                    return false;
                }
                cand_col = 0;
            }
        }

        // If no grids were written, then we must be in a separator line.
        // Don't update the row counters.
        if (grid_col == 0) {
            continue;
        }

        // After each candidate cell row, we know we want to move on to the
        // next one. If we exhaust the number of candidate rows that means
        // we want to move onto the next grid row.
        if (++cand_row >= SUDOKU_BOX_ROWS) {
            // Exhausted the number of grid rows?
            if (++grid_row >= SUDOKU_GRID_ROWS) {
                return false;
            }
            cand_row = 0;
        }
    }
    return true;
}

int
main(void)
{
    sudoku_Game G;
    repl_State  R;

    // Don't pass the string literal directly as we want to mutate it.
    // So save it into a mutable buffer beforehand.
    static char buffer[] = REPR_ASCII;
    repl_init(&R, stdin, stdout, buffer, sizeof(buffer));
    if (!sudoku_init_string(&G, sample_easy, sizeof(sample_easy) - 1)) {
        fprintf(R.output, "Invalid board received.\n");
        return 1;
    }
    return repl_run(&G, &R);
}
