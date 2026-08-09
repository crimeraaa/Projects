#ifndef SUDOKU_H
#define SUDOKU_H

#include <limits.h>

/* BEGIN: CONFIGURABLE ==================================================={{{ */

/* Grid information. */
#define SUDOKU_LINE_LENGTH          9
#define SUDOKU_BOX_ROWS             3
#define SUDOKU_BOX_COLS             3
#define SUDOKU_GRID_ROWS            SUDOKU_LINE_LENGTH
#define SUDOKU_GRID_COLS            SUDOKU_LINE_LENGTH

/* Backing type information. */
#define SUDOKU_LIMB_TYPE        unsigned int
#define SUDOKU_CELL_BITS        4
#define SUDOKU_CELL_MIN         1
#define SUDOKU_CELL_MAX         9
#define SUDOKU_CELLSET_TYPE     unsigned short

/*
 Description:
    This is the template character in the default string representation that
    is to be replaced with the actual value of each cell. It should not appear
    in other contexts within the said representation.
 */
#define SUDOKU_REPR_GRID_CHAR   'x'

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

/* END: CONFIGURABLE   ===================================================}}} */

#define SUDOKU_OK                    1
#define SUDOKU_UNSOLVABLE            0
#define SUDOKU_TERMINATED           -1

/* Non-user-configurable grid information. */
#define SUDOKU_GRID_SIZE            (SUDOKU_GRID_ROWS * SUDOKU_GRID_COLS)
#define SUDOKU_GRID_CELL_BITS       (SUDOKU_CELL_BITS * SUDOKU_GRID_SIZE)
#define SUDOKU_GRID_CELLSET_BITS    (SUDOKU_CELLSET_BITS * SUDOKU_GRID_SIZE)

/* Non-user-configurable backing type information. */
#define SUDOKU_LIMB_BITS            ((sizeof(SUDOKU_LIMB_TYPE) * CHAR_BIT))
#define SUDOKU_LIMB_COUNT(N)        (((N) / SUDOKU_LIMB_BITS) + 1)
#define SUDOKU_LIMB_CELL_COUNT      SUDOKU_LIMB_COUNT(SUDOKU_GRID_CELL_BITS)
#define SUDOKU_LIMB_CELLSET_COUNT   SUDOKU_LIMB_COUNT(SUDOKU_GRID_CELLSET_BITS)
#define SUDOKU_CELL_MASK            ((1 << SUDOKU_CELL_BITS) - 1)
#define SUDOKU_CELLSET_BITS         SUDOKU_CELL_MAX
#define SUDOKU_CELLSET_MAX          ((1 << SUDOKU_CELLSET_BITS) - 1)
#define SUDOKU_CELLSET_MASK         SUDOKU_CELLSET_MAX

typedef SUDOKU_LIMB_TYPE        sudoku_Limb;
typedef SUDOKU_CELLSET_TYPE     sudoku_CellSet;
typedef struct sudoku_Game      sudoku_Game;
typedef struct sudoku_Repr      sudoku_Repr;

struct sudoku_Game {
    sudoku_Repr *R;

    /*
     Cells are stored in a column-major fashion.
     */
    sudoku_Limb grid_cells[SUDOKU_LIMB_CELL_COUNT];

    /*
     Each row and column maps to a bit set. Here, each bit represents
     a particular number. When this bit is `1`, it indicates that said number
     could be in this cell. Otherwise, a bit of `0` indicates said number
     could not possibly be in this cell.

     Here is what the 'all' bit-set looks like, in big-endian representation:

     Bit Index          .......8_76543210
     Represented Value  .......9_87654321
                        00000001_11111111

     */
    sudoku_Limb grid_allowed[SUDOKU_LIMB_CELLSET_COUNT];
};

struct sudoku_Repr {
    /*
     Fixed-size, nul-terminated buffer for the string representation of the
     grid. On initialization, it should contain a single unique character that
     represents a cell, e.g. `x`.

     The locations of each character are to be saved. This will enable
     quick modification of the grid.
     */
    char *grid_buffer;

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
    int grid_line_count;

    /*
     Map each row and column to an index in the string representation.
     This allows us to mutate them easily when updating said representation.
     */
    int grid_indexes[SUDOKU_GRID_ROWS][SUDOKU_GRID_COLS];
};

void
sudoku_init(sudoku_Game *G, sudoku_Repr *R);

int
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
    SUDOKU_OK         - We successfully solved the given board.
    SUDOKU_UNSOLVABLE - The given board doesn't have a solution.
 */
int
sudoku_solve(sudoku_Game *G, int *step_count);

/*
 Returns a boolean:
    1 - Keep stepping through.
    0 - Quit immediately regardless of the solution state.
 */
typedef int (*sudoku_StepFn)(sudoku_Game *G, void *user_data, int step_count);

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
sudoku_solve_stepwise(sudoku_Game *G,
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
