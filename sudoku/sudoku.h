#ifndef SUDOKU_H
#define SUDOKU_H

#include <limits.h>  /* CHAR_BIT */
#include <stdbool.h> /* bool, true, false */
#include <stdint.h>  /* uint\d+_t */
#include <stddef.h>  /* size_t   */

/* BEGIN: CONFIGURABLE ==================================================={{{ */

/* Grid information. */
#define SUDOKU_LINE_LENGTH  9
#define SUDOKU_BOX_ROWS     3
#define SUDOKU_BOX_COLS     3
#define SUDOKU_GRID_ROWS    SUDOKU_LINE_LENGTH
#define SUDOKU_GRID_COLS    SUDOKU_LINE_LENGTH

/* Backing type information. */
#define SUDOKU_DIGIT_BITS   4
#define SUDOKU_DIGIT_MIN    1
#define SUDOKU_DIGIT_MAX    9

/*
 Description:
    Digit type. An unsigned integer type that can at least hold the minimum
    up to the maximum Sudoku digit values in binary.

    E.g. if our Sudoku digits go from 1 to 9, then we need at least 4 bits
    (0b0001 to 0b1001) to represent them all. The zero value (0b0000)
    represents an empty cell.
 */
#define SUDOKU_DIGIT_TYPE    uint8_t

/*'
 Description:
    Bit set that can hold all a combination of all possible digit values as
    bits to indicate their presence or absence for a particular cell.

    It should at least contain a number of bits equivalent to the number of
    possible digit values. E.g. if our Suduoku digits go from 1 to 9, then
    each digit set must be 9 bits. The zero value (0b0_00000000) represents
    no possible values for this cell. The complement thereof (0b1_1111111)
    represents a cell that can take on lala possible digit values.

    The C standard guarantees that `short` and its counterparts are at least
    16 bits, so for Sudoku up to 16x16 this is a safe bet.
 */
#define SUDOKU_DIGITSET_TYPE uint16_t

/*
 Description:
    Bit set backing type. An unsigned integer type that can at least contain
    either all possible digit value.

    E.g. for a 9x9 Sudoku, we have 4-bit digits we need at least 8 digits
    per limb. However it's usually better to have a backing type that can
    store multiple of them, e.g. a 32-bit integer can store 8 digits.

    The C standard guarantees that `int` and its counterparts are at least
    16 bits in size, so this is a safe bet to store both digit and digit sets.

 NOTE(2026-08-10):
    Ensure that the size of this type is a multiple of the digit bit count!
    Otherwise, digits will have to be 'split' across multiple limbs, which
    we do not handle at all.
 */
#define SUDOKU_LIMB_TYPE    uint32_t

/* END: CONFIGURABLE   ===================================================}}} */

#define SUDOKU_OK                    1
#define SUDOKU_UNSOLVABLE            0
#define SUDOKU_TERMINATED           -1

/* Non-user-configurable grid information. */
#define SUDOKU_BOX_SIZE              (SUDOKU_BOX_ROWS      * SUDOKU_BOX_COLS)
#define SUDOKU_GRID_SIZE             (SUDOKU_GRID_ROWS     * SUDOKU_GRID_COLS)
#define SUDOKU_GRID_DIGIT_BITS       (SUDOKU_DIGIT_BITS    * SUDOKU_GRID_SIZE)
#define SUDOKU_GRID_DIGITSET_BITS    (SUDOKU_DIGITSET_BITS * SUDOKU_GRID_SIZE)

/* Non-user-configurable backing type information. */
#define SUDOKU_LIMB_BITS             ((sizeof(SUDOKU_LIMB_TYPE) * CHAR_BIT))
#define SUDOKU_LIMB_COUNT(N)         (((N) / SUDOKU_LIMB_BITS) + 1)
#define SUDOKU_LIMB_DIGIT_COUNT      SUDOKU_LIMB_COUNT(SUDOKU_GRID_DIGIT_BITS)
#define SUDOKU_LIMB_DIGITSET_COUNT   SUDOKU_LIMB_COUNT(SUDOKU_GRID_DIGITSET_BITS)
#define SUDOKU_DIGIT_MASK            ((1 << SUDOKU_DIGIT_BITS) - 1)
#define SUDOKU_DIGITSET_BITS         SUDOKU_DIGIT_MAX
#define SUDOKU_DIGITSET_ALL          ((1 << SUDOKU_DIGITSET_BITS) - 1)

typedef SUDOKU_DIGIT_TYPE           sudoku_Digit;
typedef SUDOKU_DIGITSET_TYPE        sudoku_DigitSet;
typedef SUDOKU_LIMB_TYPE            sudoku_Limb;
typedef struct sudoku_Game          sudoku_Game;

struct sudoku_Game {
    /*
     The Sudokua grid's digits are stored, bitwise, in a column-major fashion.
     Row and column indices must be adjusted as such.
     */
    sudoku_Limb grid_digits[SUDOKU_LIMB_DIGIT_COUNT];

    /*
     Each row and column maps to a bit set. For simplicity we use a plain
     2-dimensional array to avoid dealing with bit fields split across limbs.
     */
    sudoku_DigitSet grid_allowed[SUDOKU_GRID_ROWS][SUDOKU_GRID_COLS];
};

void
sudoku_init(sudoku_Game *G);

bool
sudoku_init_string(sudoku_Game *G, char const *s, size_t n);

/*
 Description:
    Retrieves the digit at the given row and column. Note that zero (0)
    indicates an absence of any value.
 */
sudoku_Digit
sudoku_get(sudoku_Game *G, int row, int col);

sudoku_DigitSet
sudoku_candidates(sudoku_Game *G, int row, int col);

/*
 Description:
    Sets the given row and column to the given digit and updates the
    state of possible candidates aross the board.

 Returns:
    `true` if nothing wrong occurred, else `false` if said candidate
    resulted in an unsolvable board.
 */
bool
sudoku_fill(sudoku_Game *G, int row, int col, sudoku_Digit cell);

/*
 Description:
    Solves the given Sudoku board in-place.

 Returns one of the following statuses:
    SUDOKU_OK         - We successfully solved the given board.
    SUDOKU_UNSOLVABLE - The given board doesn't have a solution.
 */
int
sudoku_solve(sudoku_Game *G);

/*
 Returns a boolean:
    1 - Keep stepping through.
    0 - Quit immediately regardless of the solution state.
 */
typedef bool (*sudoku_StepFn)(sudoku_Game *G, void *user_data, int row, int col);
/*
 Description:
    Solves the given Sudoku board in-place while tracking some information
    during each step-through.

 Returns one of the following statuses:
    SUDOKU_OK         - We successfully solved the given board.
    SUDOKU_UNSOLVABLE - The given board doesn't have a solution.
    SUDOKU_TERMINATED - The callback step function told us to terminate early.
 */
int
sudoku_solve_stepwise(sudoku_Game *G, sudoku_StepFn step_fn, void *user_data);

bool
sudoku_grid_is_valid(sudoku_Game *G);

bool
sudoku_digit_is_valid(sudoku_Game *G, int row, int col, sudoku_Digit value);

#endif /* SUDOKU_H */
