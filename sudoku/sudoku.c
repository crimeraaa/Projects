// standard
#include <string.h> // memset

// local
#include "sudoku.h"

#define cast(T)                 (T)

#if defined(__GNUC__)
#define SUDOKU_UNREACHABLE()    __builtin_unreachable()
#elif defined(_MSC_VER)
#define SUDOKU_UNREACHABLE()    __assume(0)
#else
#define SUDOKU_UNREACHABLE()    ((void)0)
#endif

static bool
sudoku_char2digit(char c, sudoku_Digit *digit)
{
    switch (c) {
    case '.':
    case '0': *digit = 0; break;
    case '1': *digit = 1; break;
    case '2': *digit = 2; break;
    case '3': *digit = 3; break;
    case '4': *digit = 4; break;
    case '5': *digit = 5; break;
    case '6': *digit = 6; break;
    case '7': *digit = 7; break;
    case '8': *digit = 8; break;
    case '9': *digit = 9; break;
    default:
        return false;
    }
    return true;
}

/*
 TODO(2026-08-10): Make more generic because I have nothing better to do
 */
static int
sudoku_get_box_peers(int row, int *out_box_row2)
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
sudoku_bitset_get_ptr(
    sudoku_Limb *limbs,
    sudoku_Limb  bit_len,
    int row, int col,
    sudoku_Limb *bit_index)
{
    size_t digit_index, limb_index;
    if (!(0 <= row && row < SUDOKU_GRID_ROWS && 0 <= col && col < SUDOKU_GRID_COLS)) {
        return NULL;
    }

    // Assume a column-major representation. That is, rows are multipliers
    // in order to resolve to 1-dimensional index..
    digit_index  = (cast(size_t)row * SUDOKU_GRID_ROWS) + cast(size_t)col;

    // The actual digit index is a multiple of the bit-count since
    // each element occupies that many bits.
    digit_index *= cast(size_t)bit_len;

    /*
     Integer division of the form `n / d` can be summarized as 'how many
     times does `n` contains `d`?`.
    
     Another way to look at it is 'How many times does `d` fit in `n`?'.

     So getting the limb index is just a matter of getting how many times
     the limb's bit size fits in the digit index. However, it's entirely
     possible that our value is 'divided' between two (2) limbs.
    */
    limb_index = digit_index / SUDOKU_LIMB_BITS;

    /*
     Modulo by a power of 2 can be optimized via bitwise AND.

     Modulo is the remainder of division. In other words, what is left over
     from the division. I.e. in 3 / 2, 1 is left over because 3 can only
     contain 2 once. This leaves 1 as the remainder.

     Likewise, the bit index is whatever remains from the division above.
     */
    *bit_index = cast(sudoku_Limb)(digit_index & (SUDOKU_LIMB_BITS - 1));
    return &limbs[limb_index];
}

static sudoku_Limb
sudoku_bitset_get(
    sudoku_Limb *limbs,
    sudoku_Limb  bit_len,
    sudoku_Limb  bit_mask,
    int row, int col)
{
    sudoku_Limb limb, bit_index;
    limb = *sudoku_bitset_get_ptr(limbs, bit_len, row, col, &bit_index);
    return (limb >> bit_index) & bit_mask;
}

static void
sudoku_bitset_set(
    sudoku_Limb *limbs,
    sudoku_Limb  bit_len,
    sudoku_Limb  bit_mask,
    int row, int col,
    sudoku_Limb  value)
{
    sudoku_Limb *limb_ptr, bit_index, mask_in, mask_out;
    limb_ptr  = sudoku_bitset_get_ptr(limbs, bit_len, row, col, &bit_index);
    mask_in   = value << bit_index;
    mask_out  = ~(bit_mask << bit_index);
    *limb_ptr = (*limb_ptr & mask_out) | mask_in;
}

static sudoku_DigitSet
sudoku_make_digitset(sudoku_Digit digit)
{
    return (1 << (cast(sudoku_DigitSet)digit - 1)) & SUDOKU_DIGITSET_ALL;
}

sudoku_Digit
sudoku_get(sudoku_Game *G, int row, int col)
{
    return cast(sudoku_Digit)sudoku_bitset_get(
        G->grid_digits,
        SUDOKU_DIGIT_BITS,
        SUDOKU_DIGIT_MASK,
        row, col);
}

sudoku_DigitSet
sudoku_candidates(sudoku_Game *G, int row, int col)
{
    return G->grid_allowed[row][col];
}

/*
 Description:
    Eliminates this digit as a candidate for the cell at the given row and
    column, as well as said cell's peers. This is mainly useful when dealing
    with cells that are already or about to be filled in.

 Returns:
    `true` if successful, otherwise `false` if eliminating this digit
    resulted in an inconsistency somewhere.
 */
static bool
sudoku_eliminate(sudoku_Game *G, int row, int col, sudoku_Digit digit)
{
    sudoku_DigitSet allowed, mask_in, mask_out;

    allowed  = G->grid_allowed[row][col];
    mask_in  = sudoku_make_digitset(digit);

    // Already eliminated this digit for this position?
    // We assume that we would've already done so for the peer cells.
    if ((allowed & mask_in) == 0) {
        return true;
    }

    mask_out = ~mask_in;
    allowed &= mask_out;
    // No more remaining candidates if we were to eliminate this candidate?
    if (allowed == 0) {
        return false;
    }

    G->grid_allowed[row][col] = allowed;

    // Mark this digit as unallowable for this entire row.
    for (int j = 0; j < SUDOKU_GRID_COLS; j++) {
        G->grid_allowed[row][j] &= mask_out;
    }

    // Mark this digit as unallowable for this entire column.
    for (int i = 0; i < SUDOKU_GRID_ROWS; i++) {
        G->grid_allowed[i][col] &= mask_out;
    }


    // Mark this digit as unallowable for the remaining cells within our box.
    // Since we already marked the entire row and column, that eliminates
    // four (4) of our neighbors. We only need to worry about the remaining
    // four (4).
    int box_row1, box_col1, box_row2, box_col2;
    box_row1 = sudoku_get_box_peers(row, &box_row2);
    box_col1 = sudoku_get_box_peers(col, &box_col2);

    G->grid_allowed[box_row1][box_col1] &= mask_out;
    G->grid_allowed[box_row1][box_col2] &= mask_out;
    G->grid_allowed[box_row2][box_col1] &= mask_out;
    G->grid_allowed[box_row2][box_col2] &= mask_out;
    return true;
}

/*
 Description:
    Sets to given row and column to the given digit without checking for
    correctness (i.e. Sudoku semantics).
 */
static void
sudoku_set_digit(sudoku_Game *G, int row, int col, sudoku_Digit digit)
{
    sudoku_bitset_set(G->grid_digits,
        SUDOKU_DIGIT_BITS,
        SUDOKU_DIGIT_MASK,
        row, col,
        cast(sudoku_Limb)digit);
}

bool
sudoku_fill(sudoku_Game *G, int row, int col, sudoku_Digit digit)
{
    bool ok = sudoku_eliminate(G, row, col, digit);
    if (ok) {
        sudoku_set_digit(G, row, col, digit);
        G->grid_allowed[row][col] = 0;
    }
    return ok;
}

void
sudoku_init(sudoku_Game *G)
{
    // Start with an empty grid.
    memset(G, 0, sizeof(*G));

    // In an empty grid, all cells can have all candidates.
    for (int row = 0; row < SUDOKU_GRID_ROWS; row++) {
        for (int col = 0; col < SUDOKU_GRID_COLS; col++) {
            G->grid_allowed[row][col] = SUDOKU_DIGITSET_ALL;
        }
    }
}

bool
sudoku_init_string(sudoku_Game *G, char const *s, size_t n)
{
    int row = 0, col = 0;
    sudoku_init(G);

    for (size_t i = 0; i < n; i++) {
        sudoku_Digit digit;
        if (!sudoku_char2digit(s[i], &digit)) {
            continue;
        }

        sudoku_set_digit(G, row, col, digit);
        if (++col >= SUDOKU_GRID_COLS) {
            if (++row > SUDOKU_GRID_ROWS) {
                return false;
            }
            col = 0;
        }
    }

    // Fill in the candidates.
    for (row = 0; row < SUDOKU_GRID_ROWS; row++) {
        for (col = 0; col < SUDOKU_GRID_COLS; col++) {
            int digit = sudoku_get(G, row, col);
            // If no digit, then we have nothing to eliminate.
            if (!digit) {
                continue;
            }
            
            if (!sudoku_fill(G, row, col, digit)) {
                return false;
            }
        }
    }
    return true;
}

typedef struct sudoku_Peers sudoku_Peers;
struct sudoku_Peers {
    sudoku_DigitSet rows[SUDOKU_GRID_COLS];
    sudoku_DigitSet cols[SUDOKU_GRID_ROWS];
    sudoku_DigitSet box[SUDOKU_BOX_ROWS][SUDOKU_BOX_COLS];
};

static void
sudoku_save_peers(sudoku_Game *G, int row, int col, sudoku_Peers *peers)
{
    // Get the candidates for each column in this row.
    for (int j = 0; j < SUDOKU_GRID_COLS; j++) {
        peers->rows[j] = G->grid_allowed[row][j];
    }

    // Get the candidates for each row in this column.
    for (int i = 0; i < SUDOKU_GRID_ROWS; i++) {
        peers->cols[i] = G->grid_allowed[i][col];
    }

    int box_row = SUDOKU_BOX_ROWS * (row / SUDOKU_BOX_ROWS);
    int box_col = SUDOKU_BOX_COLS * (col / SUDOKU_BOX_COLS);
    for (int i = 0; i < SUDOKU_BOX_ROWS; i++) {
        for (int j = 0; j < SUDOKU_BOX_COLS; j++) {
            peers->box[i][j] = G->grid_allowed[box_row + i][box_col + j];
        }
    }
}

static void
sudoku_restore_peers(sudoku_Game *G, int row, int col, sudoku_Peers *peers)
{
    for (int j = 0; j < SUDOKU_GRID_COLS; j++) {
        G->grid_allowed[row][j] = peers->rows[j];
    }

    for (int i = 0; i < SUDOKU_GRID_ROWS; i++) {
        G->grid_allowed[i][col] = peers->cols[i];
    }

    int box_row = SUDOKU_BOX_ROWS * (row / SUDOKU_BOX_ROWS);
    int box_col = SUDOKU_BOX_COLS * (col / SUDOKU_BOX_COLS);
    for (int i = 0; i < SUDOKU_BOX_ROWS; i++) {
        for (int j = 0; j < SUDOKU_BOX_COLS; j++) {
            peers->box[i][j] = G->grid_allowed[box_row + i][box_col + j];
        }
    }
}

static int
sudoku_solve_backtrack(sudoku_StepFn step_fn, sudoku_Game *G, void *user_data)
{
    for (int row = 0; row < SUDOKU_GRID_ROWS; row++) {
        for (int col = 0; col < SUDOKU_GRID_COLS; col++) {
            // Skip already-occupied cells when attempting a backtrack...
            if (sudoku_get(G, row, col)) {
                continue;
            }

            // Backtracking proper.
            for (int digit = SUDOKU_DIGIT_MIN; digit <= SUDOKU_DIGIT_MAX; digit++) {
                sudoku_Peers saved_peers;
                sudoku_save_peers(G, row, col, &saved_peers);

                // If we didn't successfully fill in the digit, then that
                // means we didn't mutate the candidates either.
                if (!sudoku_fill(G, row, col, digit)) {
                    continue;
                }

                if (step_fn && !(*step_fn)(G, user_data, row, col)) {
                    return SUDOKU_TERMINATED;
                }

                // Recurse to check this guess. If we receive 0, that
                // indicates we should try another guess.
                if (sudoku_solve_backtrack(step_fn, G, user_data)) {
                    return SUDOKU_OK;
                }
                sudoku_set_digit(G, row, col, 0);
                sudoku_restore_peers(G, row, col, &saved_peers);
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
sudoku_solve(sudoku_Game *G)
{
    return sudoku_solve_stepwise(G, /*step_fn=*/NULL, /*user_data=*/NULL);
}

int
sudoku_solve_stepwise(sudoku_Game *G, sudoku_StepFn step_fn, void *user_data)
{
    return sudoku_solve_backtrack(step_fn, G, user_data);
}

bool
sudoku_grid_is_valid(sudoku_Game *G)
{
    for (int row = 0; row < SUDOKU_GRID_ROWS; row++) {
        for (int col = 0; col < SUDOKU_GRID_COLS; col++) {
            int digit = sudoku_get(G, row, col);
            if (!sudoku_digit_is_valid(G, row, col, digit)) {
                return false;
            }
        }
    }
    return true;
}

bool
sudoku_digit_is_valid(sudoku_Game *G, int row, int col, sudoku_Digit digit)
{
    sudoku_DigitSet allowed, mask_in;
    allowed = G->grid_allowed[row][col];
    mask_in = sudoku_make_digitset(digit);

    // We assume that if it's a candidate here, then our peers don't
    // invalidate it.
    return allowed & mask_in;
}

#undef cast
#undef SUDOKU_UNREACHABLE
