// standard
#include <stdio.h>  // printf
#include <string.h> // memset
#include <vcruntime.h>

// local
#include "sudoku.h"

#define cast(T)     (T)

#if defined(__GNUC__)
#define SUDOKU_UNREACHABLE()    __builtin_unreachable()
#elif defined(_MSC_VER)
#define SUDOKU_UNREACHABLE()    __assume(0)
#else
#define SUDOKU_UNREACHABLE()    ((void)0)
#endif

typedef struct sudoku_Io sudoku_Io;
struct sudoku_Io {
    FILE *input;
    FILE *output;
    int   prev_option;
};

static char const sample_easy[] =
    " . . 5 | . 1 . | . 9 8 "
    " 6 . 9 | . . . | . . . "
    " . 4 . | 2 . . | . 5 6 "
    "-------+-------+-------"
    " 2 8 6 | . 4 . | 5 3 . "
    " 5 . . | 3 9 . | . . . "
    " . . . | . 5 2 | . . . "
    "-------+-------+-------"
    " 3 6 1 | 4 . 8 | 9 . . "
    " 8 . . | 5 7 3 | . 6 4 "
    " 4 . . | . . . | . 2 . "
;

static char const sample_hard[] =
    " . 6 . | 8 . . | . 5 . "
    " . . . | 3 . 2 | 9 . . "
    " 8 . 3 | 4 . 6 | . . . "
    "-------+-------+-------"
    " . . 5 | . . . | 6 . . "
    " 1 . . | . . . | . 2 4 "
    " 3 2 . | . . . | . 8 . "
    "-------+-------+-------"
    " . 1 . | . . 3 | . . . "
    " 4 . . | 2 . 7 | . . . "
    " . 7 . | . . . | . . . "
;

static char const sample_vicious[] =
    " 4 7 . | . . 5 | . . . "
    " 5 . 1 | . 7 . | 2 . . |"
    " . 2 . | . . . | . . 1 "
    "-------+-------+-------"
    " . . . | . . . | 9 7 . "
    " 1 . . | . . 3 | 5 . . "
    " . 5 . | . 4 . | 6 . . "
    "-------+-------+-------"
    " . . . | 6 5 7 | . . . "
    " . 6 5 | 3 . . | . . . "
    " 3 . . | . 2 . | . . . "
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
enum erase_Mode {
    ERASE_END,
    ERASE_BEGIN,
    ERASE_ENTIRE,
};

/*
 Arguments for `mode`:
    ERASE_END    - Clear from cursor to the *end* of the line.
    ERASE_BEGIN  - Clear from cursor to the *beginning* of the line.
    ERASE_ENTIRE - Clear the *entire* line.
 */
static void
erase_line(FILE *stream, enum erase_Mode mode)
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
erase_display(FILE *stream, enum erase_Mode mode)
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

    *input_len = n;
    return input;
}

static int
read_option(sudoku_Io *io)
{
    char const *input;
    size_t      input_len;

    for (;;) {
        fprintf(io->output, "Continue? (y/n) ");
        input = read_line(io->input, &input_len);
        if (input_len == 1) switch (input[0]) {
        case 'y':
        case 'Y':
            io->prev_option = 1;
            return 1;
        case 'n':
        case 'N':
            return 0;
        }

        // Null input indicates EOF. We want to exit.
        if (!input) {
            return 0;
        }

        // Non-null input that is just an empty string, along with
        // a previously saved option, indicates repeat said option.
        if (input_len == 0 && io->prev_option) {
            return 1;
        }

        // Otherwise, said non-null input is an invalid option.
        // Since the stdout cursor got placed on a new line,
        // move up one line in order to erase the prompt and input.
        erase_previous_lines(io->output, 1);

        // Non-empty (i.e. length is non-zero) input means an invalid option.
        // It's possible to have non-null but empty (i.e. length is zero) input.
        //
        // E.g. the user just typed <Enter>, stdin only contains the newline.
        // This gets trimmed so we only see an empty string. In that case we
        // just want to re-prompt without mentioning the error.
        if (input_len != 0) {
            fprintf(io->output, "Unknown option '%s'. ", input);
        }
    }
    SUDOKU_UNREACHABLE();
    return 0;
}

static int
sudoku_step(sudoku_Game *G, void *user_data, int step_count)
{
    sudoku_Io * io   = cast(sudoku_Io *)user_data;
    char const *repr = sudoku_to_string(G, /*out_len=*/NULL);
    printf("%sSteps taken so far: %i.\n", repr, step_count);

    if (read_option(io)) {
        // Erase the step counter information and the prompt/input.
        // They are variably sized so this helps erase old data we may
        // not overwrite on subsequent calls.
        erase_previous_lines(io->output, 2);

        // Move the cursor back to the upper left of the grid so we can
        // overwrite it. Since we assume the grid is always the same size
        // across calls, we don't need to erase it since we'll always
        // successfully write on top of old data.
        cursor_previous_line(io->output, G->R->grid_line_count);
        return 1;
    }
    return 0;
}

static int
sudoku_repl(sudoku_Game *G, sudoku_Io *io)
{
    char const *repr = sudoku_to_string(G, /*out_len=*/NULL);
    printf("%s", repr);
    if (read_option(io)) {
        int step_count = 0;

        // Erase prompt with user input to avoid leaving behind old data.
        erase_previous_lines(io->output, 1);

        // Move cursor back to the upper left of the printed grid.
        cursor_previous_line(io->output, G->R->grid_line_count);
        sudoku_solve_stepwise(G, sudoku_step, io, &step_count);
    }
    return 0;
}

int
main(void)
{
    sudoku_Game G;
    sudoku_Repr R;
    sudoku_Io   io = {/*input_stream=*/stdin, /*output_stream=*/stdout, 0};

    // Don't pass the string literal directly as we want to mutate it.
    // So save it into a mutable buffer beforehand.
    static char buffer[] = SUDOKU_REPR_GRID_STRING;
    sudoku_repr_init(&R, buffer, sizeof(buffer), SUDOKU_REPR_GRID_CHAR);
    sudoku_init_string(&G, &R, sample_easy, sizeof(sample_easy) - 1);
    return sudoku_repl(&G, &io);
}

static int
sudoku_char2int(char c)
{
    switch (c) {
    case '.':
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    }
    return -1;
}

static char
sudoku_int2char(int cell)
{
    switch (cell) {
    case 0: return ' ';
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
    SUDOKU_UNREACHABLE();
    return 0;
}

void
sudoku_init(sudoku_Game *G, sudoku_Repr *R)
{
    G->R = R;
    // Start with an empty grid.
    memset(G->grid_limbs, 0, sizeof(G->grid_limbs));

}

void
sudoku_init_string(sudoku_Game *G, sudoku_Repr *R, char const *s, size_t n)
{
    int row = 0, col = 0;
    sudoku_init(G, R);
    for (size_t i = 0; i < n; i++) {
        int cell = sudoku_char2int(s[i]);
        if (cell == -1) {
            continue;
        }

        sudoku_set(G, row, col, cell);
        if (++col >= SUDOKU_GRID_COLS) {
            row++;
            col = 0;
        }
    }
}

int
sudoku_repr_init(sudoku_Repr *R, char *buffer, size_t len, char target)
{
    char *grid_iter;
    // Buffer must be non-empty and nul-terminated.
    if (!(buffer && len > 0 && buffer[len - 1] == 0)) {
        return 0;
    }

    // Grid string representation information.
    R->grid_buffer     = buffer;
    R->grid_buffer_len = len;
    R->grid_line_count = 0;

    // Track where we encounter the cell formatter characters and/or newlines.
    grid_iter = buffer;
    for (int row = 0; row < SUDOKU_GRID_ROWS; row++) {
        for (int col = 0; col < SUDOKU_GRID_COLS; col++) {
            while (*grid_iter != 0 && *grid_iter != target) {
                if (*grid_iter == '\n') {
                    R->grid_line_count++;
                }
                grid_iter++;
            }

            R->grid_indexes[row][col] = cast(int)(grid_iter - buffer);
            grid_iter++;
        }
    }

    while (*grid_iter != 0) {
        if (*grid_iter == '\n') {
            R->grid_line_count++;
        }
        grid_iter++;
    }
    return 1;
}

static sudoku_Limb *
sudoku_get_limb(sudoku_Game *G, int row, int col, sudoku_Limb *bit_index)
{
    sudoku_Limb cell_index, limb_index;
    if (!(0 <= row && row < SUDOKU_GRID_ROWS) || !(0 <= col && col < SUDOKU_GRID_COLS)) {
        return NULL;
    }

    // Assume a column-major representation.
    // Convert 2-dimensional coordinates into a 1-dimensional index.
    // This should be in the inclusive range [0,81].
    cell_index = (cast(sudoku_Limb)row * SUDOKU_GRID_ROWS) + cast(sudoku_Limb)col;

    // Each cell N-bits long, so the cell's actual starting bit index is a
    // multiple of N.
    cell_index *= SUDOKU_CELL_BITS;

    // Since we divide the (N*R*C)-bit array into 'limbs' (i.e. smaller-sized
    // integers that compose a larger one), determine which one we live in.
    limb_index  = cell_index / SUDOKU_LIMB_BITS;

    // Of that limb, determine which bit we start at via modulo wrap-around.
    // Modulo by power of 2 is faster using bitwise AND.
    *bit_index  = cell_index & (SUDOKU_LIMB_BITS - 1);
    return &G->grid_limbs[limb_index];
}

int
sudoku_get(sudoku_Game *G, int row, int col)
{
    sudoku_Limb *limb, bit_index;
    limb = sudoku_get_limb(G, row, col, &bit_index);
    return cast(int)((*limb >> bit_index) & SUDOKU_CELL_MASK);
}

void
sudoku_set(sudoku_Game *G, int row, int col, int value)
{
    sudoku_Limb *limb, bit_index, mask_in, mask_out;

    limb     = sudoku_get_limb(G, row, col, &bit_index);
    mask_in  = cast(sudoku_Limb)value << bit_index;

    // Use this to clear out the original value without messing up
    // any of the other bits.
    mask_out = ~(cast(sudoku_Limb)SUDOKU_CELL_MASK << bit_index);

    // Clear out the original value's bits, without messing up any of the
    // other ones, before setting the new value into place.
    *limb    = (*limb & mask_out) | mask_in;
}

static int
sudoku_solve_fn(
    sudoku_StepFn step_fn,
    sudoku_Game * G,
    void *        user_data,
    int *         step_count)
{
    for (int row = 0; row < SUDOKU_GRID_ROWS; row++) {
        for (int col = 0; col < SUDOKU_GRID_COLS; col++) {
            // Skip occupied cells when attempting a backtrack...
            if (sudoku_get(G, row, col)) {
                continue;
            }

            // Backtracking proper.
            for (int cell = 1; cell <= SUDOKU_CELL_MAX; cell++) {
                // Skip guesses that couldn't possibly work here.
                if (!sudoku_cell_is_valid(G, row, col, cell)) {
                    continue;
                }

                sudoku_set(G, row, col, cell);
                *step_count += 1;
                if (step_fn && !step_fn(G, user_data, *step_count)) {
                    return -1;
                }

                // Recurse to check this guess. If we receive 0, that
                // indicates we should try another guess.
                if (sudoku_solve_fn(step_fn, G, user_data, step_count)) {
                    return 1;
                }
                sudoku_set(G, row, col, 0);
            }

            // For our outermost backtrack call, reaching here means there is
            // no valid solution for this Sudoku board.
            //
            // Otherwise, recursive backtrack calls (child) can reach here to
            // indicate that their outer backtrack call (parent) was invalid.
            return 0;
        }
    }
    return 1;
}

int
sudoku_solve(sudoku_Game *G)
{
    return sudoku_solve_stepwise(G, NULL, NULL, NULL);
}

int
sudoku_solve_stepwise(sudoku_Game *G,
    sudoku_StepFn step_fn,
    void *        user_data,
    int *         step_count)
{
    // Avoid the need to constantly check for null.
    int tmp = 0;
    if (!step_count) {
        step_count = &tmp;
    }
    return sudoku_solve_fn(step_fn, G, user_data, step_count);
}

int
sudoku_is_valid(sudoku_Game *G)
{
    for (int row = 0; row < SUDOKU_GRID_ROWS; row++) {
        for (int col = 0; col < SUDOKU_GRID_COLS; col++) {
            int cell = sudoku_get(G, row, col);
            if (!sudoku_cell_is_valid(G, row, col, cell)) {
                return 0;
            }
        }
    }
    return 1;
}

int
sudoku_cell_is_valid(sudoku_Game *G, int row, int col, int value)
{
    // In this row, check all its child columns for conflicts.
    for (int i = 0; i < SUDOKU_GRID_COLS; i++) {
        // Ignore ourselves, because we're obviously equal to it!
        if (i == col) {
            continue;
        }

        int row_cell = sudoku_get(G, row, i);
        if (value == row_cell) {
            return 0;
        }
    }

    // In this column, check all its child rows for conflicts.
    for (int j = 0; j < SUDOKU_GRID_ROWS; j++) {
        if (j == row) {
            continue;
        }

        int col_cell = sudoku_get(G, j, col);
        if (value == col_cell) {
            return 0;
        }
    }

    int box_row_start, box_col_start;
    box_row_start = SUDOKU_BOX_ROWS * (row / SUDOKU_BOX_ROWS);
    box_col_start = SUDOKU_BOX_COLS * (col / SUDOKU_BOX_COLS);
    for (int i = 0; i < SUDOKU_BOX_ROWS; i++) {
        for (int j = 0; j < SUDOKU_BOX_COLS; j++) {
            int box_row, box_col, box_cell;

            box_row = box_row_start + i;
            box_col = box_col_start + j;
            if (box_row == row && box_col == col) {
                continue;
            }

            box_cell = sudoku_get(G, box_row, box_col);
            if (value == box_cell) {
                return 0;
            }
        }
    }
    return 1;
}

char const *
sudoku_to_string(sudoku_Game *G, size_t *out_len)
{
    sudoku_Repr *R = G->R;
    for (int row = 0; row < SUDOKU_GRID_ROWS; row++) {
        for (int col = 0; col < SUDOKU_GRID_COLS; col++) {
            int  i = R->grid_indexes[row][col];
            char c = sudoku_int2char(sudoku_get(G, row, col));
            R->grid_buffer[i] = c;
        }
    }

    if (out_len) {
        *out_len = R->grid_buffer_len - 1; 
    }
    return R->grid_buffer;
}

#undef cast
