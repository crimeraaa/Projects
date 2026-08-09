#ifndef SUDOKU_H
#define SUDOKU_H

#include <limits.h>

/*=== BEGIN: CONFIGURABLE ================================================{{{ */

#define SUDOKU_LIMB_TYPE    unsigned int

/*
 Description:
    This is the template character in the default string representation that
    is to be replaced with the actual value of each cell. It should not appear
    in other contexts within the said representation.
 */
#define SUDOKU_REPR_GRID_CHAR  'x'

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
#define SUDOKU_REPR_GRID_STRING               \
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

/*=== END:   CONFIGURABLE   ==============================================}}} */

typedef SUDOKU_LIMB_TYPE sudoku_Limb;

/* Cells are in the inclusive 4-bit range [0b0000, 0b1001] */
#define SUDOKU_CELL_BITS    4
#define SUDOKU_CELL_MIN     0
#define SUDOKU_CELL_MAX     9
#define SUDOKU_CELL_MASK    0xf

#define SUDOKU_LINE_LENGTH  9
#define SUDOKU_BOX_ROWS     3
#define SUDOKU_BOX_COLS     3
#define SUDOKU_GRID_ROWS    SUDOKU_LINE_LENGTH
#define SUDOKU_GRID_COLS    SUDOKU_LINE_LENGTH
#define SUDOKU_GRID_SIZE    (SUDOKU_GRID_ROWS * SUDOKU_GRID_COLS)
#define SUDOKU_GRID_BITS    (SUDOKU_GRID_SIZE * SUDOKU_CELL_BITS)
#define SUDOKU_LIMB_BITS    (sizeof(sudoku_Limb) * CHAR_BIT)

/*
 We assume that integer division results in a floored quotient,
 so the grid's bit size divided by the limb's bit size is always
 one off. E.g:

 324 bits /  8 bits = (40 limbs / grid) *  8 bits = 320 bits
 324 bits / 16 bits = (20 limbs / grid) * 16 bits = 320 bits
 324 bits / 32 bits = (10 limbs / grid) * 32 bits = 320 bits
 324 bits / 64 bits =  (5 limbs / grid) * 64 bits = 320 bits

 So we always need one more limb to validly store the grid in its entirety.
 */
#define SUDOKU_LIMB_COUNT   ((SUDOKU_GRID_BITS / SUDOKU_LIMB_BITS) + 1)

typedef struct sudoku_Game sudoku_Game;
typedef struct sudoku_Repr sudoku_Repr;
struct sudoku_Game {
    sudoku_Repr *R;

    /*
     Cells are stored in a column-major fashion.
     */
    sudoku_Limb  grid_limbs[SUDOKU_LIMB_COUNT];
};

struct sudoku_Repr {
    /*
     Fixed-size, nul-terminated buffer for the string representation of the
     grid. On initialization, it should contain a single unique character that
     represents a cell, e.g. `x`.

     The locations of each character are to be saved. This will enable
     quick modification of the grid.
     */
    char * grid_buffer;

    /*
     Since we assume the buffer is nul-terminated, the actual string length
     is this minus one (1).
     */
    size_t grid_buffer_len;

    /*
     Save how many newlines are stored in the string representation.
     This is mainly useful when working with ANSI escape sequences so that
     this many lines can be erased in order to redraw the grid.
     */
    int  grid_line_count;

    /*
     Map each row and column to an index in the string representation.
     This allows us to mutate them easily when updating said representation.
     */
    int  grid_indexes[SUDOKU_GRID_ROWS][SUDOKU_GRID_COLS];
};

void
sudoku_init(sudoku_Game *G, sudoku_Repr *R);

void
sudoku_init_string(sudoku_Game *G, sudoku_Repr *R, char const *s, size_t n);

int
sudoku_repr_init(sudoku_Repr *R, char *buffer, size_t len, char target);

int
sudoku_get(sudoku_Game *G, int row, int col);

void
sudoku_set(sudoku_Game *G, int row, int col, int value);

/*
 Description:
    Solves the given Sudoku board in-place.

 Returns one of the following statuses:
    1 - We successfully solved the given board.
    0 - The given board doesn't have a solution.
 */
int
sudoku_solve(sudoku_Game *G);

/*
 Returns a boolean:
    1 - Keep stepping through.
    0 - Quit immediately regardless of the solution state.
 */
typedef int (*sudoku_StepFn)(
    sudoku_Game *G,
    void *       user_data,
    int          step_count);

/*
 Description:
    Solves the given Sudoku board in-place while tracking some information
    during each step-through.

 Returns one of the following statuses:
     1 - We successfully solved the given board.
     0 - The given board doesn't have a solution.
    -1 - The callback step function told us to terminate early.
 */
int
sudoku_solve_stepwise(
    sudoku_Game * G,
    sudoku_StepFn step_fn,
    void *        user_data,
    int *         step_count);

int
sudoku_is_valid(sudoku_Game *G);

int
sudoku_cell_is_valid(sudoku_Game *G, int row, int col, int value);

char const *
sudoku_to_string(sudoku_Game *G, size_t *out_len);

#endif /* SUDOKU_H */
