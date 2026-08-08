// standard
#include <stdio.h>  // printf
#include <string.h> // memset

// local
#include "sudoku.h"

#define cast(T)     (T)

static char const sample_easy[] =
    "'-----------------------`"
    "| . . 5 | . 1 . | . 9 8 |"
    "| 6 . 9 | . . . | . . . |"
    "| . 4 . | 2 . . | . 5 6 |"
    "|-------+-------+-------|"
    "| 2 8 6 | . 4 . | 5 3 . |"
    "| 5 . . | 3 9 . | . . . |"
    "| . . . | . 5 2 | . . . |"
    "|-------+-------+-------|"
    "| 3 6 1 | 4 . 8 | 9 . . |"
    "| 8 . . | 5 7 3 | . 6 4 |"
    "| 4 . . | . . . | . 2 . |"
    "`-----------------------'"
;

static char const sample_hard[] =
    "'-----------------------`"
    "| . 6 . | 8 . . | . 5 . |"
    "| . . . | 3 . 2 | 9 . . |"
    "| 8 . 3 | 4 . 6 | . . . |"
    "|-------+-------+-------|"
    "| . . 5 | . . . | 6 . . |"
    "| 1 . . | . . . | . 2 4 |"
    "| 3 2 . | . . . | . 8 . |"
    "|-------+-------+-------|"
    "| . 1 . | . . 3 | . . . |"
    "| 4 . . | 2 . 7 | . . . |"
    "| . 7 . | . . . | . . . |"
    "`-----------------------'"
;

static char const sample_vicious[] =
    "'-----------------------`"
    "| 4 7 . | . . 5 | . . . |"
    "| 5 . 1 | . 7 . | 2 . . |"
    "| . 2 . | . . . | . . 1 |"
    "|-------+-------+-------|"
    "| . . . | . . . | 9 7 . |"
    "| 1 . . | . . 3 | 5 . . |"
    "| . 5 . | . 4 . | 6 . . |"
    "|-------+-------+-------|"
    "| . . . | 6 5 7 | . . . |"
    "| . 6 5 | 3 . . | . . . |"
    "| 3 . . | . 2 . | . . . |"
    "`-----------------------'"
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

static char const *
read_line(FILE *stream, size_t *input_len)
{
    static char buf[256];
    char *input;
    size_t n = 0;

    input = fgets(buf, sizeof(buf), stream);
    if (input) {
        n        = strcspn(input, "\r\n");
        input[n] = 0;
    }

    *input_len = n;
    return input;
}

static int
sudoku_repl(sudoku_Game *G)
{
    int step_count = 0;
    for (;;) {
        char const *repr, *input;
        size_t      repr_len, input_len;

        repr = sudoku_to_string(G, &repr_len);
        printf("%sSteps taken to solve: %i.\n", repr, step_count);

repl_ask:
        fprintf(stdout, "Continue? (y/n) ");

        // Note that this also affects the stdout cursor, usually.
        // It gets placed on the new line as well.
        input = read_line(stdin, &input_len);
        if (input_len == 1) switch (input[0]) {
        case 'y':
        case 'Y':
            if (!sudoku_solve(G, &step_count)) {
                fprintf(stdout, "[ERROR] The given board is unsolvable!\n");
                return 1;
            }
            break;
        case 'n':
        case 'N':
            fputc('\n', stdout);
            return 0;
        default:
            goto repl_error;
        } else {
repl_error:
            // Non-null input?
            if (input) {
                // Since the stdout cursor got placed on a new line,
                // move up one line in order to erase the prompt and input.
                cursor_previous_line(stdout, 1);
                erase_line(stdout, ERASE_ENTIRE);

                // Non-null and non-empty input means an invalid option.
                if (input_len != 0) {
                    fprintf(stdout, "Unknown option '%s'. ", input);
                }

                // It's possible to have non-null but empty input, e.g. the
                // user just typed <Enter>. Stdin only contains the newline,
                // which gets trimmed.
                goto repl_ask;
            }

            // Otherwise, got null input. We want to exit.
            return 0;
        }

        // Erase the prompt and input.
        cursor_previous_line(stdout, 1);
        erase_line(stdout, ERASE_ENTIRE);

        // Erase the step counter information.
        cursor_previous_line(stdout, 1);
        erase_line(stdout, ERASE_ENTIRE);

        // Move the cursor to the start of the grid.
        cursor_previous_line(stdout, G->grid_line_count);
    }
    return 0;
}

int
main(void)
{
    sudoku_Game G;
    sudoku_init_string(&G, sample_vicious, sizeof(sample_vicious) - 1);
    return sudoku_repl(&G);
}

#if defined(__GNUC__)
#define SUDOKU_UNREACHABLE()    __builtin_unreachable()
#elif defined(_MSC_VER)
#define SUDOKU_UNREACHABLE()    __assume(0)
#else
#define SUDOKU_UNREACHABLE()    ((void)0)
#endif

static int
sudoku_char2int(char c)
{
    switch (c) {
    case '.': return 0;
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
sudoku_init(sudoku_Game *G)
{
    char *grid_iter;

    // Start with an empty grid.
    memset(G->grid_limbs, 0, sizeof(G->grid_limbs));

    // Grid string representation information.
    G->grid_line_count = 0;
    memcpy(G->grid_string, SUDOKU_GRID_STRING, sizeof(SUDOKU_GRID_STRING));

    // Track where we encounter the cell formatter characters and/or newlines.
    grid_iter = G->grid_string;
    for (int row = 0; row < SUDOKU_ROWS; row++) {
        for (int col = 0; col < SUDOKU_COLS; col++) {
            while (*grid_iter != 0 && *grid_iter != 'x') {
                if (*grid_iter == '\n') {
                    G->grid_line_count++;
                }
                grid_iter++;
            }

            G->grid_indexes[row][col] = cast(int)(grid_iter - G->grid_string);
            grid_iter++;
        }
    }

    while (*grid_iter != 0) {
        if (*grid_iter == '\n') {
            G->grid_line_count++;
        }
        grid_iter++;
    }
}

void
sudoku_init_string(sudoku_Game *G, char const *input, size_t input_len)
{
    int row = 0, col = 0;
    sudoku_init(G);
    for (size_t i = 0; i < input_len; i++) {
        int cell = sudoku_char2int(input[i]);
        if (cell == -1) {
            continue;
        }

        sudoku_set(G, row, col, cell);
        if (++col >= SUDOKU_COLS) {
            row++;
            col = 0;
        }
    }

}

static sudoku_Limb *
sudoku_get_limb(sudoku_Game *G, int row, int col, sudoku_Limb *bit_index)
{
    sudoku_Limb cell_index, limb_index;
    if (!(0 <= row && row < SUDOKU_ROWS) || !(0 <= col && col < SUDOKU_COLS)) {
        return NULL;
    }

    // Assume a column-major representation.
    // Convert 2-dimensional coordinates into a 1-dimensional index.
    // This should be in the inclusive range [0,81].
    cell_index = (cast(sudoku_Limb)row * SUDOKU_ROWS) + cast(sudoku_Limb)col;

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

int
sudoku_solve(sudoku_Game *G, int *step_count)
{
    for (int row = 0; row < SUDOKU_ROWS; row++) {
        for (int col = 0; col < SUDOKU_COLS; col++) {
            // Skip occupied cells when attempting a backtrack...
            if (sudoku_get(G, row, col)) {
                continue;
            }

            // Backtracking proper.
            for (int cell = 1; cell <= SUDOKU_CELL_MAX; cell++) {
                if (!sudoku_cell_is_valid(G, row, col, cell)) {
                    continue;
                }

                sudoku_set(G, row, col, cell);
                *step_count += 1;
                if (sudoku_solve(G, step_count)) {
                    return 1;
                }
                sudoku_set(G, row, col, 0);
            }

            // For our outermost backtrack call, reaching here means there is
            // no valid solution for this sudoku.
            //
            // Otherwise, recursive backtrack calls (child) can reach here to
            // indicate that their outer backtrack call (parent) was invalid.
            return 0;
        }
    }
    return 1;
}

int
sudoku_is_valid(sudoku_Game *G)
{
    for (int row = 0; row < SUDOKU_ROWS; row++) {
        for (int col = 0; col < SUDOKU_COLS; col++) {
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
    for (int i = 0; i < SUDOKU_COLS; i++) {
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
    for (int j = 0; j < SUDOKU_ROWS; j++) {
        if (j == row) {
            continue;
        }

        int col_cell = sudoku_get(G, j, col);
        if (value == col_cell) {
            return 0;
        }
    }

    int box_row_start, box_col_start;
    box_row_start = SUDOKU_BOX_LENGTH * (row / SUDOKU_BOX_LENGTH);
    box_col_start = SUDOKU_BOX_HEIGHT * (col / SUDOKU_BOX_HEIGHT);
    for (int i = 0; i < SUDOKU_BOX_LENGTH; i++) {
        for (int j = 0; j < SUDOKU_BOX_HEIGHT; j++) {
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
sudoku_to_string(sudoku_Game *G, size_t *n)
{
    for (int row = 0; row < SUDOKU_ROWS; row++) {
        for (int col = 0; col < SUDOKU_COLS; col++) {
            int  i = G->grid_indexes[row][col];
            char c = sudoku_int2char(sudoku_get(G, row, col));
            G->grid_string[i] = c;
        }
    }

    if (n) {
        *n = sizeof(G->grid_string) - 1;
    }
    return G->grid_string;
}

#undef cast
