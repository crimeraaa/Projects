// standard
#include <stdio.h>  // fgets, fputc, fputs, fprintf
#include <string.h> // memset

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


typedef enum {
    COMMAND_NONE,
    COMMAND_QUIT,
    COMMAND_NEXT,
    COMMAND_FINISH,
} Command;

typedef struct sudoku_Io sudoku_Io;
struct sudoku_Io {
    FILE *  input;
    FILE *  output;
    Command prev_command;
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
    " 5 . 1 | . 7 . | 2 . . "
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

    *input_len = n;
    return input;
}

static Command
read_command(sudoku_Io *io)
{
    char const *input;
    size_t      input_len;
    Command     cmd = COMMAND_NONE;

    for (;;) {
        fprintf(io->output, "Option ([Ff]inish, [Nn]ext, [Qq]uit): ");
        input = read_line(io->input, &input_len);
        if (input_len == 1) switch (input[0]) {
        case 'F':
        case 'f': cmd = COMMAND_FINISH; break;
        case 'N':
        case 'n': cmd = COMMAND_NEXT;   break;
        case 'Q':
        case 'q': cmd = COMMAND_QUIT;   break;
        }

        // Null input indicates EOF. We want to exit.
        if (!input) {
            cmd = COMMAND_QUIT;
        }

        if (cmd) {
            io->prev_command = cmd;
            return cmd;
        }

        // Non-null input that is just an empty string, along with
        // a previously saved option, indicates repeat said option.
        if (input_len == 0 && io->prev_command) {
            return io->prev_command;
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

static void
sudoku_repl_erase(sudoku_Game *G, sudoku_Io *io, int extra_line_count)
{
    // Erase, for example, step counter information and/or prompt with input.
    // They are variably sized so this helps erase old data we may
    // not overwrite on subsequent calls.
    erase_previous_lines(io->output, extra_line_count);

    // Move the cursor back to the upper left of the grid so we can
    // overwrite it. Since we assume the grid is always the same size
    // across calls, we don't need to erase it since we'll always
    // successfully write on top of old data.
    cursor_previous_line(io->output, G->R->grid_line_count);
}

static int
sudoku_repl_step(sudoku_Game *G, void *user_data, int step_count)
{
    sudoku_Io * io;
    char const *repr;
    Command     cmd;

    io = cast(sudoku_Io *)user_data;
    if (io->prev_command == COMMAND_FINISH) {
        return 1;
    }

    repr = sudoku_to_string(G, /*out_len=*/NULL);
    fprintf(io->output, "%sSteps taken so far: %i.\n", repr, step_count);
    cmd  = read_command(io);
    switch (cmd) {
    case COMMAND_NONE:
    case COMMAND_QUIT:
        return 0;
    case COMMAND_NEXT:
    case COMMAND_FINISH:
        sudoku_repl_erase(G, io, 2);
        return 1;
    }
    return 0;
}

static int
sudoku_repl(sudoku_Game *G, sudoku_Io *io)
{
    char const *repr;
    Command     cmd;
    int         step_count = 0;
    int         status     = 0;

    repr = sudoku_to_string(G, /*out_len=*/NULL);
    fprintf(io->output, "%s", repr);
    cmd  = read_command(io);
    switch (cmd) {
    case COMMAND_NONE:
    case COMMAND_QUIT: return 0;
    case COMMAND_NEXT:
        sudoku_repl_erase(G, io, 1);
        status = sudoku_solve_stepwise(G, sudoku_repl_step, io, &step_count);
        break;
    case COMMAND_FINISH:
        sudoku_repl_erase(G, io, 1);
        status = sudoku_solve(G, &step_count);
        break;
    }

    switch (status) {
    case SUDOKU_OK:
        repr = sudoku_to_string(G, NULL);
        fprintf(io->output, "%sSteps taken: %i.\n", repr, step_count);
        break;
    case SUDOKU_UNSOLVABLE:
        repr = sudoku_to_string(G, NULL);
        fprintf(io->output, "%sNo solution found.\n", repr);
        break;
    case SUDOKU_TERMINATED:
        break;
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
    if (!sudoku_init_string(&G, &R, sample_easy, sizeof(sample_easy) - 1)) {
        fprintf(io.output, "Invalid board received.\n");
        return 1;
    }
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
    // Start with an empty grid.
    memset(G, 0, sizeof(*G));
    G->R = R;
}

/*
 TODO(2026-08-10): Make more generic because I have nothing better to do
 */
static int
sudoku_get_box_neighbors(int row, int *out_box_row2)
{
    int box_row1, box_row2;
    switch (row) {
    case 0:
    case 3:
    case 6:
        box_row1 = row + 1;
        box_row2 = row + 2;
        break;
    case 1:
    case 4:
    case 7:
        box_row1 = row - 1;
        box_row2 = row + 1;
        break;
    case 2:
    case 5:
    case 8:
        box_row1 = row - 2;
        box_row2 = row - 1;
        break;
    }
    *out_box_row2 = box_row2;
    return box_row1;
}

static sudoku_Limb *
sudoku_bitset_get_limb(
    sudoku_Limb *limbs,
    sudoku_Limb  bit_len,
    int row, int col,
    sudoku_Limb *bit_index)
{
    sudoku_Limb row_index, cell_index, limb_index;
    if (!(0 <= row && row < SUDOKU_GRID_ROWS && 0 <= col && col < SUDOKU_GRID_COLS)) {
        return NULL;
    }

    // Assume a column-major representation. That is, rows are multipliers
    // in order to resolve to 1-dimensional index..
    row_index   = cast(sudoku_Limb)row * SUDOKU_GRID_ROWS;
    cell_index  = row_index + cast(sudoku_Limb)col;

    // The actuall cell index is a multiple of the bit count since
    // each element occupies that many bits.
    cell_index *= bit_len;

    /*
     Division of the form `n / d` can be summarized as 'how many times does
     `n` contains `d`?`.
    
     Another way to look at it is 'How many times does `d` fit in `n`?'.

     So getting the limb index is just a matter of getting how many times
     the limb's bit size fits in the cell index.
    */
    limb_index  = cell_index / SUDOKU_LIMB_BITS;

    /*
     Modulo by a power of 2 can be optimized via bitwise AND.

     Modulo is the remainder of division. In other words, what is left over
     from the division. I.e. in 3 / 2, 1 is left over because 3 can only
     contain 2 once. This leaves 1 as the remainder.

     LIkewise, the bit index is whatever remains from the division above.
     */
    *bit_index  = cell_index & (SUDOKU_LIMB_BITS - 1);
    return &limbs[limb_index];
}

static sudoku_Limb
sudoku_bitset_get(
    sudoku_Limb *limbs,
    sudoku_Limb  bit_len,
    sudoku_Limb  bit_mask,
    int row, int col)
{
    sudoku_Limb *limb, bit_index;
    limb = sudoku_bitset_get_limb(limbs, bit_len, row, col, &bit_index);
    return cast(sudoku_Limb)((*limb >> bit_index) & bit_mask);
}

static void
sudoku_bitset_set(
    sudoku_Limb *limbs,
    sudoku_Limb  bit_len,
    sudoku_Limb  bit_mask,
    int row, int col,
    sudoku_Limb  value)
{
    sudoku_Limb *limb, bit_index, mask_in, mask_out;
    limb     = sudoku_bitset_get_limb(limbs, bit_len, row, col, &bit_index);
    mask_in  = value << bit_index;
    mask_out = ~(bit_mask << bit_index);
    *limb = (*limb & mask_out) | mask_in;
}

static sudoku_CellSet
sudoku_get_allowed(sudoku_Game *G, int row, int col)
{
    return cast(sudoku_CellSet)sudoku_bitset_get(
        G->grid_allowed,
        SUDOKU_CELLSET_BITS,
        SUDOKU_CELLSET_MASK,
        row, col);
}

static void
sudoku_set_allowed(sudoku_Game *G, int row, int col, sudoku_CellSet value)
{
    sudoku_bitset_set(G->grid_allowed,
        SUDOKU_CELLSET_BITS,
        SUDOKU_CELLSET_MASK,
        row, col,
        cast(sudoku_Limb)value);
}

int
sudoku_get(sudoku_Game *G, int row, int col)
{
    return cast(int)sudoku_bitset_get(
        G->grid_cells,
        SUDOKU_CELL_BITS,
        SUDOKU_CELL_MASK,
        row, col);
}

void
sudoku_set(sudoku_Game *G, int row, int col, int value)
{
    sudoku_bitset_set(G->grid_cells,
        SUDOKU_CELL_BITS,
        SUDOKU_CELL_MASK,
        row, col,
        cast(sudoku_Limb)value);
}


static void
sudoku_propagate_allowed_values(sudoku_Game *G, int row, int col, int cell)
{
    sudoku_CellSet mask_out = ~(1 << (cell - 1)) & SUDOKU_CELLSET_MAX;

    // Mark this cell as unallowable for this entire column.
    for (int i = 0; i < SUDOKU_GRID_ROWS; i++) {
        sudoku_set_allowed(G, i, col, mask_out);
    }

    // Mark this cell as unallowable for this entire row.
    for (int j = 0; j < SUDOKU_GRID_COLS; j++) {
        sudoku_set_allowed(G, row, j, mask_out);
    }


    // Mark this cell as unallowable for the remaining cells within our box.
    // Since we already marked the entire row and column, that eliminates
    // four (4) of our neighbors. We only need to worry about the remaining
    // four (4).
    int box_row1, box_col1, box_row2, box_col2;
    box_row1 = sudoku_get_box_neighbors(row, &box_row2);
    box_col1 = sudoku_get_box_neighbors(col, &box_col2);

    sudoku_set_allowed(G, box_row1, box_col1, mask_out);
    sudoku_set_allowed(G, box_row1, box_col2, mask_out);
    sudoku_set_allowed(G, box_row2, box_col1, mask_out);
    sudoku_set_allowed(G, box_row2, box_col2, mask_out);
}

int
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

    // Fill in the candidates.
    for (row = 0; row < SUDOKU_GRID_ROWS; row++) {
        for (col = 0; col < SUDOKU_GRID_COLS; col++) {
            int cell = sudoku_get(G, row, col);
            if (cell) {
                sudoku_set_allowed(G, row, col, 0);
                sudoku_propagate_allowed_values(G, row, col, cell);
                continue;
            }
            
            // Start off by assuming all empty cells can possibly hold all
            // numbers.
            sudoku_set_allowed(G, row, col, SUDOKU_CELLSET_MAX);
        }
    }
    return 1;
}

int
sudoku_repr_init(sudoku_Repr *R, char *buffer, size_t len, char target)
{
    char *grid_iter;
    // Buffer must fit the grid at least and be nul-terminated.
    if (!(buffer && len > SUDOKU_GRID_SIZE && buffer[len - 1] == 0)) {
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

            // Ran out of buffer, indicating we don't have enough space
            // to format the grid into.
            if (*grid_iter == 0) {
                return 0;
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

static int
sudoku_solve_recurse(
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
            for (int cell = SUDOKU_CELL_MIN; cell <= SUDOKU_CELL_MAX; cell++) {
                // Skip guesses that couldn't possibly work here.
                if (!sudoku_cell_is_valid(G, row, col, cell)) {
                    continue;
                }

                sudoku_set(G, row, col, cell);
                *step_count += 1;
                if (step_fn && !step_fn(G, user_data, *step_count)) {
                    return SUDOKU_TERMINATED;
                }

                // Recurse to check this guess. If we receive 0, that
                // indicates we should try another guess.
                if (sudoku_solve_recurse(step_fn, G, user_data, step_count)) {
                    return SUDOKU_OK;
                }
                sudoku_set(G, row, col, 0);
            }

            // For our outermost backtrack call, reaching here means there is
            // no valid solution for this Sudoku board.
            //
            // Otherwise, recursive backtrack calls (child) can reach here to
            // indicate that their outer backtrack call (parent) was invalid.
            return SUDOKU_UNSOLVABLE;
        }
    }
    return SUDOKU_OK;
}

int
sudoku_solve(sudoku_Game *G, int *step_count)
{
    return sudoku_solve_stepwise(G, /*step_fn=*/NULL, /*user_data=*/NULL, step_count);
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
    return sudoku_solve_recurse(step_fn, G, user_data, step_count);
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

    int box_row1, box_row2, box_col1, box_col2;
    box_row1 = sudoku_get_box_neighbors(row, &box_row2);
    box_col1 = sudoku_get_box_neighbors(col, &box_col2);
    // Yes, I'm aware this is hot garbage
    if (sudoku_get(G, box_row1, box_col1) == value
        || sudoku_get(G, box_row1, box_col2) == value
        || sudoku_get(G, box_row2, box_col1) == value
        || sudoku_get(G, box_row2, box_col2) == value) {
        return 0;
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
